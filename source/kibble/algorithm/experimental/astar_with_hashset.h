#pragma once

#include "../assert/assert.h"
#include "../logger2/channel.h"
#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "fmt/format.h"

/*
 * This is a rework of the A* algorithm implemented in algorithm/astar.h
 * One major difference is the use of a priority_queue combined with a hash set instead
 * of a vector-based min-heap to implement the open set. A priority_queue cannot erase a
 * random element, and can only be updated by active removal / re-insertion. By allowing
 * nodes in the priority_queue to become invalid (lazily-removed during pop operation),
 * and by maintaining a list of valid nodes in the hash set, open set search and update
 * can in theory be further optimized.
 *
 * However, a benchmark shows that the present approach is significantly slower than the
 * original (measured only on the maze, avg search time is 30% faster for 1e6 shots in
 * release build). It looks like the extra overhead is not worth it for small enough
 * graphs.
 */

namespace kb::experimental
{

namespace detail
{
constexpr inline size_t round_up_pow2(int32_t base, int32_t multiple)
{
    return size_t((base + multiple - 1) & -multiple);
}

/**
 * @internal
 * @brief Simple node pool using an intrusive linked-list
 *
 * @tparam T
 */
template <typename T>
class NodePool
{
public:
    struct Element
    {
        T data;
        // Intrusive linked list
        Element* next{nullptr};
    };

    static constexpr size_t k_align = 8;
    static constexpr size_t k_node_size = round_up_pow2(sizeof(Element), k_align);
    static constexpr size_t k_default_count = 128;

    NodePool(size_t max_nodes = k_default_count)
    {
        // Allocate (aligned) block
        size_t block_size = max_nodes * k_node_size;
        block_ = new (std::align_val_t(k_align)) uint8_t[block_size];
        std::memset(block_, 0, block_size);
        head_ = reinterpret_cast<Element*>(block_);

        // Setup free list
        for (size_t ii = 0; ii < max_nodes - 1; ++ii)
        {
            // NOTE(ndx): Elements are aligned
            Element* p_elem = reinterpret_cast<Element*>(block_ + k_node_size * ii);
            p_elem->next = reinterpret_cast<Element*>(block_ + k_node_size * (ii + 1));
        }
    }

    ~NodePool()
    {
        operator delete[](block_, std::align_val_t(k_align));
    }

    T* allocate()
    {
        // Return null if no more entry left
        if (head_ == nullptr)
            return nullptr;

        ++allocation_count_;
        // Obtain one element from the head of the free list
        Element* p_elem = head_;
        head_ = p_elem->next;
        return &p_elem->data;
    }

    void deallocate(void* ptr)
    {
        --allocation_count_;
        // Put the returned element at the head of the free list
        Element* p_elem = reinterpret_cast<Element*>(ptr);
        p_elem->next = head_;
        head_ = p_elem;
    }

    inline size_t allocation_count() const
    {
        return allocation_count_;
    }

private:
    uint8_t* block_{nullptr};
    Element* head_{nullptr};
    size_t allocation_count_{0};
};
} // namespace detail

/**
 * @brief Describes a type that can be used as a template parameter for Astar.
 *
 * Requirements:
 * - a comparison operator for equality
 * - a 64-bit `hash` function
 * - a `transition_cost` method that calculates the cost of moving from this state to another
 * - a `heuristic` method that estimates the remaining distance to the goal state (must never overestimate -> merely
 * admissible)
 * - a `get_successors` method that returns a list of states that can be reached from this state
 *
 * The search state must be copy-constructible. Write a destructor if you perform any heap allocations,
 * it will be called when the corresponding node is deallocated by the algorithm.
 *
 * @tparam T
 */
template <typename T>
concept AstarState = requires(T state, const T& other, const T* other_ptr) {
    {
        state == other
    } -> std::same_as<bool>;
    {
        state.hash()
    } -> std::convertible_to<size_t>;
    {
        state.transition_cost(other)
    } -> std::convertible_to<float>;
    {
        state.heuristic(other)
    } -> std::convertible_to<float>;
    {
        state.get_successors(other_ptr)
    } -> std::convertible_to<std::vector<T>>;
};

/**
 * @brief [EXPERIMENTAL] Perform A* search on a graph.
 *
 * @warning Use the non-experimental version in production!
 *
 * @tparam T User state type
 */
template <AstarState T>
class Astar
{
private:
    /**
     * @internal
     * @brief Allocatable node with user state and A* data
     *
     */
    struct Node
    {
        T state;               // User state
        float g_score{0.f};    // Cost of reaching this node + cost of this node
        float h_score{0.f};    // Heuristic estimate of remaining distance to goal state
        float f_score{0.f};    // g + h
        bool valid{true};      // Invalid nodes are skipped during open set pop operation
        Node* parent{nullptr}; // Keep track of parents in order to guide search
        Node* next{nullptr};   // For solution path after convergence

        Node(const T& state_) : state(state_)
        {
        }

        inline bool operator==(const Node& other) const
        {
            return state == other.state;
        }

        inline void update(Node* parent_, float g_score_, float h_score_)
        {
            parent = parent_;
            g_score = g_score_;
            h_score = h_score_;
            f_score = g_score_ + h_score_;
        }
    };

public:
    /// @brief The status of the search
    enum class Status
    {
        RUNNING = 0,
        SUCCESS,
        FAILURE,
    };

    struct Statistics
    {
        size_t steps{0};
        size_t nodes_created{0};
        size_t node_invalidations{0};
        size_t node_reopenings{0};
        size_t lazy_removals{0};
    };

    /**
     * @brief Construct and fully initialize an A* search object
     *
     * @param start Start state
     * @param goal Goal state
     * @param max_nodes Max number of nodes to allocate in pool
     */
    Astar(const T& start, const T& goal, size_t max_nodes = 128) : pool_(max_nodes)
    {
        start_ = create_node(start);
        goal_ = create_node(goal);
        start_->update(nullptr, 0.f, start_->state.heuristic(goal));

        // Push start node to open set
        push_open(start_);
    }

    ~Astar()
    {
        // Only the nodes in the solution path are alive at this point
        if (status_ == Status::SUCCESS)
            for (Node* node = start_; node; node = node->next)
                destroy_node(node);

        K_ASSERT(pool_.allocation_count() == 0, "Node pool leaked memory.", log_channel_)
            .watch_var__(pool_.allocation_count(), "#allocations");
    }

    /**
     * @brief Set a logger channel for assertions.
     *
     * @param log_channel
     */
    inline void set_logger_channel(const log::Channel* log_channel)
    {
        log_channel_ = log_channel;
    }

    /**
     * @brief Perform search.
     *
     * A status code is returned to indicate success or failure.
     * If the search was successful, the solution path can be visited thanks
     * to the `walk_path` method.
     * The search can be cancelled any time by returning `true` from the
     * `cancel_request` predicate.
     *
     * @param cancel_request Stops search if `true` is returned
     * @return Status of search
     */
    inline Status search(std::function<bool(const Astar<T>&)> cancel_request = [](const Astar<T>&) { return false; })
    {
        while (step(cancel_request) == Status::RUNNING)
            ;
        return status_;
    }

    /// @brief Get statistics
    inline const Statistics& get_statistics() const
    {
        return stats_;
    }

    /// @brief Get the total cost of the solution
    inline float get_solution_cost() const
    {
        return solution_cost_;
    }

    /**
     * @brief Execute a function on each state in the solution path
     *
     * The path is traversed from the start state to the goal state.
     *
     * @param visitor Visitor function
     */
    inline void walk_path(std::function<void(const T&)> visitor)
    {
        for (Node* node = start_; node; node = node->next)
            visitor(node->state);
    }

private:
    /**
     * @internal
     * @brief Advance to the next best node.
     *
     * @param cancel_request Exits early if `true` is returned
     * @return Status
     */
    Status step(std::function<bool(const Astar<T>&)> cancel_request)
    {
        // If search already converged, return early
        if (status_ != Status::RUNNING)
            return status_;

        // Failure to pop from open set means no solution
        // Also handle cancel requests from user
        if (open_hash_.empty() || cancel_request(*this))
        {
            free_all_nodes();
            status_ = Status::FAILURE;
            return status_;
        }

        ++stats_.steps;

        // Pop node with lower f-score
        // NOTE(ndx): We're guaranteed to have a valid node, because the open hash set is non-empty
        Node* node = pop_open();

        // Have we reached the goal?
        if (node->state == goal_->state)
        {
            // Remove duplicate goal node and reconstruct path
            goal_->parent = node->parent;
            goal_->g_score = node->g_score;
            destroy_node(node);
            reconstruct_path();
            free_unused_nodes();
            status_ = Status::SUCCESS;
            return status_;
        }

        // Goal not reached yet. We generate the successors
        std::vector<T> successor_states = node->state.get_successors(node->parent ? &node->parent->state : nullptr);

        // Analyze successors
        for (const T& suc_state : successor_states)
        {
            // Cumulative cost of reaching this successor state
            float g_score = node->g_score + node->state.transition_cost(suc_state);

            // Dummy node to search by key (faster than using std::find_if in the search methods)
            Node dummy(suc_state);

            // Node is already in closed set
            if (Node* p_closed_node = find_in_closed_set(&dummy); p_closed_node)
            {
                // New g-score is no better, no need to update, skip
                if (p_closed_node->g_score <= g_score)
                    continue;

                /*
                    NOTE(ndx): We don't know if the heuristic is 'consistent' (monotonically decreasing),
                    we only assume it is 'merely admissible' (never overestimates). So we can't be sure that
                    the optimal path to this state is the first followed, and we can't simply ignore the successor.
                    Thus, we need to update it and re-open it (which kills time complexity).
                    In practice, it is very likely that an admissible heuristic is consistent as well,
                    in which case this code path is unreachable.
                */

                // Update node
                p_closed_node->update(node, g_score, suc_state.heuristic(goal_->state));

                // Re-open node
                push_open(p_closed_node);
                closed_set_.erase(p_closed_node);
                ++stats_.node_reopenings;
            }
            // Node is already in open set
            else if (Node* p_open_node = find_in_open_set(&dummy); p_open_node)
            {
                // New g-score is no better, no need to update, skip
                if (p_open_node->g_score <= g_score)
                    continue;

                // Invalidate and re-insert
                invalidate_open(p_open_node);
                Node* p_updated = create_node(suc_state);
                p_updated->update(node, g_score, suc_state.heuristic(goal_->state));
                push_open(p_updated);
                ++stats_.node_invalidations;
            }
            // New successor
            else
            {
                // Create proper node and initialize it
                Node* successor = create_node(suc_state);
                successor->update(node, g_score, suc_state.heuristic(goal_->state));

                // Push to open set
                push_open(successor);
            }
        }

        // Close node, as we have explored it
        closed_set_.insert(node);
        return status_;
    }

    /**
     * @internal
     * @brief Form the solution path as a (doubly-)linked list of nodes.
     *
     */
    void reconstruct_path()
    {
        Node* next = goal_;
        Node* parent = goal_->parent;

        // It may happen that the goal node is the start node, in which case the path is trivial
        while (*next != *start_)
        {
            parent->next = next;
            next = parent;
            parent = parent->parent;
        }

        // Also retrieve solution cost
        solution_cost_ = goal_->g_score;
    }

    /**
     * @internal
     * @brief Allocate and construct a new node.
     *
     * @param state State to be copied to the new node
     * @return Node*
     */
    Node* create_node(const T& state)
    {
        // Pool allocation
        Node* address = pool_.allocate();

        K_ASSERT(address != nullptr, "Out of memory.", log_channel_)
            .watch_var__(pool_.allocation_count(), "#allocations");

        // In-place construction and initialization
        Node* node = new (address) Node(state);
        ++stats_.nodes_created;
        return node;
    }

    /**
     * @internal
     * @brief Destroy a node.
     *
     * @note No check is performed on the node address, it is assumed to be valid.
     *
     * @param node
     */
    inline void destroy_node(Node* node)
    {
        node->~Node();
        pool_.deallocate(node);
    }

    /**
     * @internal
     * @brief Destroy all nodes created so far.
     *
     */
    void free_all_nodes()
    {
        while (!open_set_.empty())
        {
            Node* node = open_set_.top();
            destroy_node(node);
            open_set_.pop();
        }

        for (Node* node : closed_set_)
            destroy_node(node);

        open_hash_.clear();
        closed_set_.clear();

        // Start node was either in open set or closed set, no worries
        // Goal node is guaranteed to be unreached at this point
        destroy_node(goal_);
    }

    /**
     * @internal
     * @brief Destroy all nodes that are non-essential for the solution path.
     *
     */
    void free_unused_nodes()
    {
        // Destroy invalid nodes, all valid nodes are in the open_hash set
        while (!open_set_.empty())
        {
            Node* node = open_set_.top();
            if (!node->valid)
                destroy_node(node);

            open_set_.pop();
        }

        for (Node* node : open_hash_)
            if (node->next == nullptr)
                destroy_node(node);

        for (Node* node : closed_set_)
            if (node->next == nullptr)
                destroy_node(node);

        open_hash_.clear();
        closed_set_.clear();
    }

    /**
     * @internal
     * @brief Insert node into the open set
     *
     * @param node
     */
    inline void push_open(Node* node)
    {
        open_hash_.insert(node);
        open_set_.push(node);
    }

    /**
     * @internal
     * @brief Make this node invalid
     *
     * It will be removed from the hash set and skipped during pop operation
     *
     * @param node
     */
    inline void invalidate_open(Node* node)
    {
        node->valid = false;
        open_hash_.erase(node);
    }

    /**
     * @internal
     * @brief Retrieve and pop top node from the open set
     *
     * @return Node*
     */
    Node* pop_open()
    {
        // Pop nodes until a valid one is found
        Node* node = open_set_.top();
        while (!node->valid)
        {
            open_set_.pop();
            // Lazy-removal of invalid nodes
            destroy_node(node);
            node = open_set_.top();
            ++stats_.lazy_removals;
        }
        open_set_.pop();
        // Remove from the hash set
        open_hash_.erase(node);
        return node;
    }

    /**
     * @internal
     * @brief Find a node with the same state in the open set
     *
     * @param node
     * @return The node pointer or nullptr
     */
    inline Node* find_in_open_set(Node* node)
    {
        auto findit = open_hash_.find(node);
        return findit != open_hash_.end() ? *findit : nullptr;
    }

    /**
     * @internal
     * @brief Find a node with the same state in the closed set
     *
     * @param node
     * @return The node pointer or nullptr
     */
    inline Node* find_in_closed_set(Node* node)
    {
        auto findit = closed_set_.find(node);
        return findit != closed_set_.end() ? *findit : nullptr;
    }

private:
    // Comparator for priority queue
    struct fComparator
    {
        inline bool operator()(const Node* lhs, const Node* rhs) const
        {
            return lhs->f_score > rhs->f_score;
        }
    };

    // For the unordered set
    struct NodeHash
    {
        inline size_t operator()(const Node* node) const
        {
            return node->state.hash();
        }
    };

    /*
        std::unordered_set avoids duplicates by comparing values, and not keys.
        We want to override this behavior and compare hashes instead.
    */
    struct NodeKeyEqual
    {
        inline bool operator()(const Node* node1, const Node* node2) const
        {
            return NodeHash{}(node1) == NodeHash{}(node2);
        }
    };

    detail::NodePool<Node> pool_;
    Node* start_{nullptr};
    Node* goal_{nullptr};
    Status status_{Status::RUNNING};
    float solution_cost_{std::numeric_limits<float>::max()};
    Statistics stats_;

    std::priority_queue<Node*, std::vector<Node*>, fComparator> open_set_;
    std::unordered_set<Node*, NodeHash, NodeKeyEqual> open_hash_;
    std::unordered_set<Node*, NodeHash, NodeKeyEqual> closed_set_;

    const log::Channel* log_channel_{nullptr};
};

} // namespace kb::experimental