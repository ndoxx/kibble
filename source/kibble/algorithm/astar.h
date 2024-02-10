#pragma once

#include "../assert/assert.h"
#include "../logger2/channel.h"
#include "../memory/memory_utils.h"
#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <memory>
#include <unordered_set>
#include <vector>

namespace kb
{

constexpr inline size_t round_up_pow2(int32_t base, int32_t multiple)
{
    return size_t((base + multiple - 1) & -multiple);
}

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

template <AstarState T>
class Astar
{
public:
    enum class Status
    {
        RUNNING = 0,
        SUCCESS,
        FAILURE,
    };

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

    // Comparator for priority queue
    struct fComparator
    {
        bool operator()(const Node* lhs, const Node* rhs) const
        {
            return lhs->f_score > rhs->f_score;
        }
    };

    Astar(const T& start, const T& goal, size_t max_nodes = 128) : pool_(max_nodes)
    {
        set_start_and_goal(start, goal);
    }

    inline void set_logger_channel(const log::Channel* log_channel)
    {
        log_channel_ = log_channel;
    }

    inline Status search(std::function<bool(const Astar<T>&)> cancel_request = [](const Astar<T>&) { return false; })
    {
        while (step(cancel_request) == Status::RUNNING)
            ;
        return status_;
    }

    inline size_t get_steps() const
    {
        return steps_;
    }

    inline void visit_path(std::function<void(const T&)> visitor)
    {
        for (Node* node = start_; node != goal_; node = node->next)
            visitor(node->state);
    }

private:
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
                rebuild_open_heap();
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
    }

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

    void destroy_node(Node* node)
    {
        node->~Node();
        pool_.deallocate(node);
    }

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

    inline void push_open_heap(Node* node)
    {
        open_set_.push_back(node);
        std::push_heap(open_set_.begin(), open_set_.end(), fComparator{});
    }

    inline Node* pop_open_heap()
    {
        Node* node = open_set_.front();
        std::pop_heap(open_set_.begin(), open_set_.end(), fComparator{});
        open_set_.pop_back();
        return node;
    }

    inline void rebuild_open_heap()
    {
        std::make_heap(open_set_.begin(), open_set_.end(), fComparator{});
    }

    inline auto find_in_open_set(const T& state)
    {
        return std::find_if(open_set_.begin(), open_set_.end(),
                            [&state](const Node* node) { return node->state == state; });
    }

    inline auto find_in_closed_set(const T& state)
    {
        return std::find_if(closed_set_.begin(), closed_set_.end(),
                            [&state](const Node* node) { return node->state == state; });
    }

    void set_start_and_goal(const T& start, const T& goal)
    {
        start_ = create_node(start);
        goal_ = create_node(goal);

        start_->h_score = start_->state.heuristic(goal);
        start_->f_score = start_->h_score; // f = g + h, g = 0

        // Push start node to open set
        push_open_heap(start_);
    }

    struct NodeHash
    {
        size_t operator()(const Node* node) const
        {
            return node->state.hash();
        }
    };

private:
    NodePool<Node> pool_;
    Node* start_{nullptr};
    Node* goal_{nullptr};
    Status status_{Status::RUNNING};
    size_t steps_{0};

    // NOTE(ndx): Open set is a vector used as a min-heap
    std::vector<Node*> open_set_;
    std::unordered_set<Node*, NodeHash> closed_set_;

    const log::Channel* log_channel_ = nullptr;
};

} // namespace kb