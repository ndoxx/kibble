#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "../hash/hash.h"

namespace kb
{
namespace undo
{
class UndoCommand;
template <typename T>
using is_undo_command = typename std::is_base_of<UndoCommand, T>;
template <typename T>
inline constexpr bool is_undo_command_v = is_undo_command<T>::value;

/**
 * @brief Represents a user action that can be undone and redone
 *
 */
class UndoCommand
{
public:
    /**
     * @brief Construct a new Undo Command object
     *
     * @param action_text Text associated to this action, for history display
     * @param merge_id Unique ID to identify commands that can be merged, set to -1 if command is not mergeable
     */
    explicit UndoCommand(const std::string &action_text, ssize_t merge_id = -1) noexcept;

    /**
     * @brief Destroy the Undo Command object
     *
     */
    virtual ~UndoCommand() = default;

    /**
     * @brief Get the text associated to this action
     *
     * @return const std::string &
     */
    inline const std::string &text() const
    {
        return action_text_;
    }

    /**
     * @brief Get the merge id
     *
     * @return ssize_t
     */
    inline ssize_t merge_id() const
    {
        return merge_id_;
    }

    /**
     * @brief Make this command obsolete.
     * 
     * An obsolete command will be destroyed when pushed to a stack.
     * This is useful when merging two commands, and the resulting command happens to have no action at all.
     *
     */
    inline void set_obsolete()
    {
        obsolete_ = true;
    }

    /**
     * @brief Check whether this command is obsolete. Called by UndoStack::push().
     *
     * @return true when the command is obsolete and can be removes
     * @return false otherwise
     */
    inline bool is_obsolete() const
    {
        return obsolete_;
    }

    /**
     * @brief Get the number of children this (macro-)command has. Macro-commands are composite objects
     * that can own multiple children sub-commands.
     *
     * @return size_t
     */
    inline size_t child_count() const
    {
        return children_.size();
    }

    /**
     * @brief Get a specific child of this macro-command
     *
     * @param index The index of the child
     * @return const UndoCommand&
     */
    inline const UndoCommand &child(size_t index)
    {
        return *children_.at(index);
    }

    /**
     * @brief Push a sub-command as a child of this command.
     * 
     * This command effectively becomes a macro-command,
     * and undoing() / redoing() it runs all its children undo() / redo() functions by default. This command
     * takes ownership of the child command which will never be mutable anymore from the outside.
     *
     * @param cmd Child command
     */
    inline void push(std::unique_ptr<UndoCommand> cmd)
    {
        children_.push_back(std::move(cmd));
    }

    /**
     * @brief Convenience function to construct a command pointer in-place before pushing it as a child.
     *
     * @tparam C Type of UndoCommand to create
     * @tparam Args Types of the arguments to be forwarded to the command constructor
     * @param args Arguments to be forwarded to the command constructor
     * @see push()
     */
    template <typename C, typename... Args, typename = std::enable_if_t<is_undo_command_v<C>>>
    inline void push(Args &&...args)
    {
        push(std::make_unique<C>(std::forward<Args>(args)...));
    }

    /**
     * @brief Rolls back the state before this command was executed.
     * @note Default implementation will call children's undo() in reverse order.
     *
     */
    virtual void undo();

    /**
     * @brief (Re-)executes this command.
     * @note Default implementation will call children's redo() in order.
     * 
     */
    virtual void redo();

    /**
     * @brief Attempt to merge another command with this command.
     *
     * This allows for command compression.
     * It is the responsibility of the programmer to override this function to specify the merging mechanism.
     * Basically, all information contained in cmd should be extracted and added to this command, in such a way that:
     * - redoing this command would be the same as redoing both unmerged commands in order
     * - undoing this command would be the same as undoing both unmerged commands in reverse order
     * If the resulting command produces no action, the programmer is free to call set_obsolete() within the
     * implementation, and its destruction will be taken care of by the UndoStack.
     *
     * @param cmd The command to merge with this one
     * @return true if the merge was successful
     * @return false otherwise
     * @note Default implementation simply returns false.
     */
    virtual bool merge_with(const UndoCommand &cmd);

protected:
    ssize_t merge_id_;

private:
    bool obsolete_ = false;
    std::string action_text_;
    std::vector<std::unique_ptr<UndoCommand>> children_;
};

/**
 * @brief Implements the undo mechanism.
 * 
 * UndoCommands can be pushed to this stack, and be rolled-back / re-executed
 * any time by calling the appropriate functions. Such a stack can also be used in conjunction with others in
 * a coordinated manner with UndoGroups.
 *
 */
class UndoStack
{
public:
    /**
     * @brief Push a command to this stack.
     * 
     * This stack takes ownership of the command which will never be mutable again
     * from the outside. Pushing a command immediately executes its redo() function. Any command that can be redone
     * before cmd is pushed will be destroyed, so the head will always match the command count. If the clean state was
     * located after head, it will be reset. Before the command is pushed however, this function will attempt to merge
     * it with the previous one on the stack (the next command to undo). If the merge is successful, cmd will be
     * destroyed. And if the merge produces an obsolete command, it will be destroyed as well. In any other case, cmd is
     * simply pushed to the stack. If the undo limit is not set to zero and the command count exceeds it, the furthest
     * command in the stack will be destroyed, so as to keep the command count constant.
     *
     * @param cmd The command to push
     */
    void push(std::unique_ptr<UndoCommand> cmd);

    /**
     * @brief Convenience function to create a command pointer in-place before pushing it to this stack.
     * This function is only enabled for classes that derive from UndoCommand.
     *
     * @tparam C Type of the command to be pushed
     * @tparam Args Types of the arguments to be forwarded to the command constructor
     * @param args Arguments to be forwarded to the command constructor
     * @see push()
     */
    template <typename C, typename... Args, typename = std::enable_if_t<is_undo_command_v<C>>>
    inline void push(Args &&...args)
    {
        push(std::make_unique<C>(std::forward<Args>(args)...));
    }

    /**
     * @brief Destroy all commands in this stack and reset its state.
     *
     */
    void clear();

    /**
     * @brief Call undo() on the command located just before the head position (if any) and decrement head.
     * If the head is at index 0 (no command can be undone), nothing will happen.
     * @see UndoCommand::undo()
     */
    void undo();

    /**
     * @brief Call redo() on the command located at head (if any) and increment head. If the head is equal
     * to the command count (no command can be redone), nothing will happen.
     * @see UndoCommand::redo()
     *
     */
    void redo();

    /**
     * @brief Call undo() or redo() iteratively until the head matches the index argument.
     *
     * Any state change will only trigger a single call to the state tracker functors.
     * The index is first clipped to the correct bounds, so calling set_head() with a target index greater than
     * the command count will only redo every command after the head position, and nothing more.
     *
     * @param index The target index
     * @see undo() redo()
     */
    void set_head(size_t index);

    /**
     * @brief Mark this state as the "clean state".
     * 
     * This is typically called when the underlying document / model
     * is saved. The clean state can be reached back through the use of the undo() / redo() functions. Anytime
     * the clean state changes, the on_clean_change functor will be called.
     *
     */
    void set_clean();

    /**
     * @brief Leave the clean state and reset the clean index to -1.
     *
     */
    void reset_clean();

    /**
     * @brief Set the maximum number of commands that can be pushed to this stack.
     * 
     * By default the undo limit is
     * set to zero, meaning that the stack can grow unbounded. Pushing a command when the stack's count reaches
     * the undo limit will cause the furthest command to be destroyed in order to maintain a constant count.
     * This function can only be called on an empty stack, or will fail and do nothing.
     *
     * @param undo_limit The target undo limit
     * @return true if the undo limit was successfully updated
     * @return false otherwise
     */
    bool set_undo_limit(size_t undo_limit);

    /**
     * @brief Get the text associated to a command on this stack, specified by index. The text is returned as a
     * string_view. If the index is out of bounds, this function will throw.
     *
     * @param index The target command index
     * @return std::string_view
     */
    std::string_view text(size_t index) const;

    /**
     * @brief Get the text associated to the next redoable command if any, or an empty string.
     *
     * @return std::string_view
     */
    std::string_view redo_text() const;

    /**
     * @brief Get the text associated to the next undoable command if any, or an empty string.
     *
     * @return std::string_view
     */
    std::string_view undo_text() const;

    /**
     * @brief Return a const reference to the command located at the specified index. This function will
     * throw if the index is out of bounds.
     *
     * @param index The target index
     * @return const UndoCommand&
     */
    inline const UndoCommand &at(size_t index) const
    {
        return *history_.at(index);
    }

    /**
     * @brief Get the number of commands in this stack.
     *
     * @return size_t
     */
    inline size_t count() const
    {
        return history_.size();
    }

    /**
     * @brief Get the current head position. This is the index of the next redoable command, or the same as
     * count() if no command can be redone. A head of 0 indicates that no command can be undone
     *
     * @return size_t
     */
    inline size_t head() const
    {
        return head_;
    }

    /**
     * @brief Get the undo limit.
     *
     * @return size_t
     */
    inline size_t limit() const
    {
        return undo_limit_;
    }

    /**
     * @brief Get the index of the command marked as clean.
     * 
     * If there is no clean state (because it was reset
     * or because it was never set in the first place), this function will return -1.
     *
     * @return ssize_t The clean state index, or -1 if no clean state is reachable.
     */
    inline ssize_t clean_index() const
    {
        return clean_index_;
    }

    /**
     * @brief Check whether this stack contains no command.
     *
     * @return true if the stack is empty
     * @return false otherwise
     */
    inline bool empty() const
    {
        return history_.empty();
    }

    /**
     * @brief Check if at least one command can be redone, meaning the head is less than count().
     *
     * @return true if a command exists at head index
     * @return false otherwise
     */
    inline bool can_redo() const
    {
        return head_ < count();
    }

    /**
     * @brief Check if at least one command can be undone, meaning the head is greater than zero.
     *
     * @return true if a command exists just before head index
     * @return false otherwise
     */
    inline bool can_undo() const
    {
        return head_ > 0;
    }

    /**
     * @brief Check if the stack is in the clean state.
     *
     * @return true if the clean index matches the head
     * @return false otherwise
     * @see set_clean()
     */
    inline bool is_clean() const
    {
        return ssize_t(head_) == clean_index_;
    }

    /**
     * @brief Set a functor to be called whenever the head moves. This functor will only get called once
     * when calling set_head(), regardless of the number of iterations performed.
     *
     * @param func
     */
    inline void on_head_change(std::function<void(size_t)> func)
    {
        on_head_change_ = func;
    }

    /**
     * @brief Set a functor to be called whenever the clean state changes.
     *
     * @param func
     */
    inline void on_clean_change(std::function<void(bool)> func)
    {
        on_clean_change_ = func;
    }

    /**
     * @brief Set a functor to be called whenever can_undo() changes.
     *
     * @param func
     */
    inline void on_can_undo_change(std::function<void(bool)> func)
    {
        on_can_undo_change_ = func;
    }

    /**
     * @brief Set a functor to be called whenever can_redo() changes.
     *
     * @param func
     */
    inline void on_can_redo_change(std::function<void(bool)> func)
    {
        on_can_redo_change_ = func;
    }

    /**
     * @brief Debug function that produces a string containing state information about this object.
     *
     * @return std::string
     */
    std::string dump() const;

private:
    struct Snapshot
    {
        size_t head = 0;
        size_t count = 0;
        bool is_clean = false;
        bool can_undo = false;
        bool can_redo = false;
    };

    // Save state to last_snapshot_, in order to facilitate diffing later on
    void snapshot();
    // Track all state changes that occurred since last snapshot, and call the relevant functors
    void check_state_transitions();
    // undo() implementation without state tracking
    void undo_internal();
    // redo() implementation without state tracking
    void redo_internal();
    // reset_clean() implementation without state tracking
    void reset_clean_internal();

private:
    std::deque<std::unique_ptr<UndoCommand>> history_;
    size_t undo_limit_ = 0;
    size_t head_ = 0;
    ssize_t clean_index_ = -1;
    Snapshot last_snapshot_;

    std::function<void(size_t)> on_head_change_ = [](size_t) {};
    std::function<void(bool)> on_clean_change_ = [](bool) {};
    std::function<void(bool)> on_can_undo_change_ = [](bool) {};
    std::function<void(bool)> on_can_redo_change_ = [](bool) {};
};

/**
 * @brief Handler that allows grouping multiple stacks, only one of which (the active stack) can be accessed
 * at a time.
 *
 */
class UndoGroup
{
public:
    /**
     * @brief Add a stack to this group.
     * 
     * The group takes complete ownership of it, and the stack will never be
     * accessible in a mutable fashion from the outside anymore.
     *
     * @param stack_name Non-null string hash uniquely identifying this stack.
     * @return true if the stack was added successfully
     * @return false otherwise
     */
    bool add_stack(hash_t stack_name);

    /**
     * @brief Remove a stack at that name.
     * 
     * If this stack was active, the active stack id will be reset. Nothing happens
     * if the stack name does not exist within this group.
     *
     * @param stack_name The target stack to destroy
     * @return true if the stack was successfully destroyed
     * @return false otherwise
     */
    bool remove_stack(hash_t stack_name);

    /**
     * @brief Set the stack corresponding to this name as the active stack. If no stack exists at that name, nothing
     * will happen.
     *
     * @param stack_name The target stack to make active
     * @return true if the stack was successfully made active
     * @return false otherwise
     */
    bool set_active(hash_t stack_name);

    /**
     * @brief Get the redo text of the active stack.
     *
     * @return std::string_view The redo text, or an empty string if no stack is active
     * @see UndoStack::redo_text()
     */
    std::string_view redo_text() const;

    /**
     * @brief Get the undo text of the active stack.
     *
     * @return std::string_view The redo text, or an empty string if no stack is active
     * @see UndoStack::undo_text()
     */
    std::string_view undo_text() const;

    /**
     * @brief Push a command into the active stack.
     *
     * @param cmd The command to push
     * @see UndoStack::push()
     */
    inline void push(std::unique_ptr<UndoCommand> cmd)
    {
        if (active_stack_)
            stacks_[active_stack_].push(std::move(cmd));
    }

    /**
     * @brief Convenience function to construct a command in-place before pushing it to the active stack.
     *
     * @tparam C Type of UndoCommand to create
     * @tparam Args Types of the arguments to be forwarded to the command constructor
     * @param args Arguments to be forwarded to the command constructor
     * @see push()
     */
    template <typename C, typename... Args, typename = std::enable_if_t<is_undo_command_v<C>>>
    inline void push(Args &&...args)
    {
        push(std::make_unique<C>(std::forward<Args>(args)...));
    }

    /**
     * @brief Clear the active stack.
     * @see UndoStack::clear()
     *
     */
    inline void clear()
    {
        if (active_stack_)
            stacks_[active_stack_].clear();
    }

    /**
     * @brief Call undo() on the active stack.
     * @see UndoStack::undo()
     *
     */
    inline void undo()
    {
        if (active_stack_)
            stacks_[active_stack_].undo();
    }

    /**
     * @brief Call redo() on the active stack.
     * @see UndoStack::redo()
     *
     */
    inline void redo()
    {
        if (active_stack_)
            stacks_[active_stack_].redo();
    }

    /**
     * @brief Call set_head() on the active stack.
     *
     * @param index The target index
     * @see UndoStack::set_head()
     */
    inline void set_head(size_t index)
    {
        if (active_stack_)
            stacks_[active_stack_].set_head(index);
    }

    /**
     * @brief Call set_clean() on the active stack.
     * @see UndoStack::set_clean()
     *
     */
    inline void set_clean()
    {
        if (active_stack_)
            stacks_[active_stack_].set_clean();
    }

    /**
     * @brief Call reset_clean() on the active stack.
     * @see UndoStack::reset_clean()
     *
     */
    inline void reset_clean()
    {
        if (active_stack_)
            stacks_[active_stack_].reset_clean();
    }

    /**
     * @brief Set the active stack's undo limit.
     *
     * @param undo_limit Undo limit setpoint
     * @return true if the undo limit was updated successfully
     * @return false otherwise
     * @see UndoStack::set_undo_limit()
     */
    inline bool set_undo_limit(size_t undo_limit)
    {
        if (active_stack_)
            return stacks_[active_stack_].set_undo_limit(undo_limit);
        return false;
    }

    /**
     * @brief Access a particular stack in a non-mutable manner.
     *
     * @param stack_name The target stack's name
     * @return const UndoStack&
     */
    inline const UndoStack &stack(hash_t stack_name) const
    {
        return stacks_.at(stack_name);
    }

    /**
     * @brief Non-mutably access the active stack.
     *
     * @return const UndoStack&
     */
    inline const UndoStack &active_stack() const
    {
        return stacks_.at(active_stack_);
    }

    /**
     * @brief Get the active stack's name.
     *
     * @return hash_t
     */
    inline hash_t active_stack_name() const
    {
        return active_stack_;
    }

    /**
     * @brief Get the total number of stacks handled by this group instance.
     *
     * @return size_t
     */
    inline size_t size() const
    {
        return stacks_.size();
    }

    /**
     * @brief Set a functor to be called whenever the active stack changes.
     *
     * @param func
     */
    inline void on_active_stack_change(std::function<void(hash_t)> func)
    {
        on_active_stack_change_ = func;
    }

    /**
     * @brief Set a functor to be called whenever the active stack's head changes, or the active stack changes itself.
     *
     * @param func
     * @see UndoStack::on_head_change()
     */
    void on_head_change(std::function<void(size_t)> func);

    /**
     * @brief Set a functor to be called whenever the active stack's clean state changes, or the active stack changes
     * itself.
     *
     * @param func
     * @see UndoStack::on_clean_change()
     */
    void on_clean_change(std::function<void(bool)> func);

    /**
     * @brief Set a functor to be called whenever the active stack's can_undo() property changes, or the active stack
     * changes itself.
     *
     * @param func
     * @see UndoStack::on_can_undo_change()
     */
    void on_can_undo_change(std::function<void(bool)> func);

    /**
     * @brief Set a functor to be called whenever the active stack's can_redo() property changes, or the active stack
     * changes itself.
     *
     * @param func
     * @see UndoStack::on_can_redo_change()
     */
    void on_can_redo_change(std::function<void(bool)> func);

private:
    // Set the active stack and call the relevant functors
    void change_active_stack(hash_t stack_name);

private:
    hash_t active_stack_ = 0;
    std::unordered_map<hash_t, UndoStack> stacks_;

    std::function<void(hash_t)> on_active_stack_change_ = [](hash_t) {};
    std::function<void(size_t)> on_head_change_ = [](size_t) {};
    std::function<void(bool)> on_clean_change_ = [](bool) {};
    std::function<void(bool)> on_can_undo_change_ = [](bool) {};
    std::function<void(bool)> on_can_redo_change_ = [](bool) {};
};

} // namespace undo
} // namespace kb