#pragma once

#include "../assert/assert.h"
#include "../logger2/channel.h"
#include "../memory/memory_utils.h"
#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <numeric>
#include <unordered_set>
#include <vector>

namespace kb
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
 * - a `heuristic` method that estimates the remaining distance to the goal state (must never overestimate)
 * - a `get_successors` method that returns a list of states that can be reached from this state
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
 * @brief Perform A* search on a graph.
 *
 * Intended for single use.
 *
 * This is heavily inspired by justinhj's astar-algorithm-cpp project on github:
 * https://github.com/justinhj/astar-algorithm-cpp
 * This implementation is more succinct, requires less user code, and uses modern C++.
 *
 * @note The open set is implemented as a vector-based min-heap with stl heap operations
 * instead of a priority_queue. This is essential to support heap update after a random
 * element modification. A priority_queue cannot erase a random element, and can only be
 * updated by active removal / re-insertion. Extending the stl priority queue to support
 * random removal would imply using make_heap under the hood, which defeats the purpose.
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

        start_->h_score = start_->state.heuristic(goal);
        start_->f_score = start_->h_score; // f = g + h, g = 0

        // Push start node to open set
        push_open_heap(start_);
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

    /// @brief Get the number of steps taken by the search
    inline size_t get_steps() const
    {
        return steps_;
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
        if (open_set_.empty() || cancel_request(*this))
        {
            free_all_nodes();
            status_ = Status::FAILURE;
            return status_;
        }

        ++steps_;

        // Pop node with lower f-score
        Node* node = pop_open_heap();

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

            // Node is already in open set
            if (auto open_it = find_in_open_set(suc_state); open_it != open_set_.end())
            {
                // New g-score is no better, no need to update, skip
                if ((*open_it)->g_score <= g_score)
                    continue;

                // Update node
                (*open_it)->update(node, g_score, suc_state.heuristic(goal_->state));

                // Heap must be re-constructed
                invalidate_open_heap();
            }
            // Node is already in closed set
            else if (auto closed_it = find_in_closed_set(suc_state); closed_it != closed_set_.end())
            {
                // New g-score is no better, no need to update, skip
                if ((*closed_it)->g_score <= g_score)
                    continue;

                // Update node
                (*closed_it)->update(node, g_score, suc_state.heuristic(goal_->state));

                // Re-open node
                push_open_heap(*closed_it);
                closed_set_.erase(closed_it);
            }
            // New successor
            else
            {
                // Create proper node and initialize it
                Node* successor = create_node(suc_state);
                successor->update(node, g_score, suc_state.heuristic(goal_->state));

                // Push to open set
                push_open_heap(successor);
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
    void destroy_node(Node* node)
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
        for (Node* node : open_set_)
            destroy_node(node);

        for (Node* node : closed_set_)
            destroy_node(node);

        open_set_.clear();
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
        for (Node* node : open_set_)
            if (node->next == nullptr)
                destroy_node(node);

        for (Node* node : closed_set_)
            if (node->next == nullptr)
                destroy_node(node);

        open_set_.clear();
        closed_set_.clear();
    }

    /**
     * @internal
     * @brief Push a new node to the open set
     *
     * @param node
     */
    inline void push_open_heap(Node* node)
    {
        open_set_.push_back(node);
        std::push_heap(open_set_.begin(), open_set_.end(), fComparator{});
    }

    /**
     * @internal
     * @brief Retrieve and pop top node from the open set
     *
     * @return Node*
     */
    inline Node* pop_open_heap()
    {
        Node* node = open_set_.front();
        std::pop_heap(open_set_.begin(), open_set_.end(), fComparator{});
        open_set_.pop_back();
        return node;
    }

    /**
     * @internal
     * @brief Rebuild the heap for the open set
     * @note This is a costly operation. It only happens when a random element
     * is updated in the open set, and there's no way around that.
     *
     */
    inline void invalidate_open_heap()
    {
        std::make_heap(open_set_.begin(), open_set_.end(), fComparator{});
    }

    /**
     * @internal
     * @brief Find a node with a given state in the open set
     *
     * @param state
     * @return An iterator to the node
     */
    inline auto find_in_open_set(const T& state)
    {
        return std::find_if(open_set_.begin(), open_set_.end(),
                            [&state](const Node* node) { return node->state == state; });
    }

    /**
     * @internal
     * @brief Find a node with a given state in the closed set
     *
     * @param state
     * @return An iterator to the node
     */
    inline auto find_in_closed_set(const T& state)
    {
        return std::find_if(closed_set_.begin(), closed_set_.end(),
                            [&state](const Node* node) { return node->state == state; });
    }

private:
    detail::NodePool<Node> pool_;
    Node* start_{nullptr};
    Node* goal_{nullptr};
    Status status_{Status::RUNNING};
    size_t steps_{0};
    float solution_cost_{std::numeric_limits<float>::max()};

    std::vector<Node*> open_set_;
    std::unordered_set<Node*, NodeHash> closed_set_;

    const log::Channel* log_channel_{nullptr};
};

} // namespace kb