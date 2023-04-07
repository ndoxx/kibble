#include "undo/undo.h"
#include <algorithm>
#include <sstream>

namespace kb
{
namespace undo
{

UndoCommand::UndoCommand(const std::string& action_text, ssize_t merge_id) noexcept
    : merge_id_(merge_id), action_text_(action_text)
{
}

void UndoCommand::undo()
{
    // Call children's undo() in reverse order
    std::for_each(children_.rbegin(), children_.rend(), [](const auto& child) { child->undo(); });
}

void UndoCommand::redo()
{
    // Call children's redo() in order
    for (const auto& child : children_)
        child->redo();
}

bool UndoCommand::merge_with(const UndoCommand&)
{
    return false;
}

bool UndoStack::set_undo_limit(size_t undo_limit)
{
    if (empty())
    {
        undo_limit_ = undo_limit;
        return true;
    }
    return false;
}

void UndoStack::push(std::unique_ptr<UndoCommand> cmd)
{
    cmd->redo();

    // Save state for state tracking purposes
    snapshot();

    // If commands have been undone, remove all commands after head
    if (can_redo())
    {
        history_.erase(std::next(history_.begin(), ssize_t(head_)), history_.end());
        // If clean state was located after head, reset it
        if (ssize_t(head_) < clean_index_)
            reset_clean_internal();
    }

    // Try to merge with previous command
    bool has_merged = false;
    if (can_undo())
    {
        auto prev_id = history_[head_ - 1]->merge_id();
        auto cmd_id = cmd->merge_id();
        // Two successive commands of the same merge id
        if (cmd_id != -1 && prev_id == cmd_id)
        {
            has_merged = history_[head_ - 1]->merge_with(*cmd);
            if (has_merged)
            {
                // cmd was merged with previous command, get rid of it
                // merge successful -> head does not move (we just "edited" the previous command)
                auto* raw = cmd.release();
                delete raw;
            }
        }
    }

    if (!has_merged)
    {
        // If limit will be exceeded, pop front command
        if ((undo_limit_ > 0) && (count() >= undo_limit_))
            history_.pop_front();

        // Push command to the stack
        history_.push_back(std::move(cmd));
    }

    // Check if command is obsolete
    if (history_.back()->is_obsolete())
    {
        if (clean_index_ >= ssize_t(count()))
            reset_clean_internal();
        history_.pop_back();
    }

    head_ = count();
    check_state_transitions();
}

void UndoStack::clear()
{
    snapshot();
    history_.clear();
    head_ = 0;
    reset_clean_internal();
    check_state_transitions();
}

void UndoStack::snapshot()
{
    last_snapshot_ = Snapshot{head(), count(), is_clean(), can_undo(), can_redo()};
}

void UndoStack::check_state_transitions()
{
    if (head() != last_snapshot_.head)
        on_head_change_(head());
    if (is_clean() != last_snapshot_.is_clean)
        on_clean_change_(is_clean());
    if (can_undo() != last_snapshot_.can_undo)
        on_can_undo_change_(can_undo());
    if (can_redo() != last_snapshot_.can_redo)
        on_can_redo_change_(can_redo());
}

void UndoStack::undo_internal()
{
    // Undo command under head and decrement head
    history_[--head_]->undo();
}

void UndoStack::redo_internal()
{
    // Redo command at head and increment head
    history_[head_++]->redo();
}

void UndoStack::reset_clean_internal()
{
    clean_index_ = -1;
}

void UndoStack::undo()
{
    if (can_undo())
    {
        snapshot();
        undo_internal();
        check_state_transitions();
    }
}
void UndoStack::redo()
{
    if (can_redo())
    {
        snapshot();
        redo_internal();
        check_state_transitions();
    }
}

void UndoStack::set_head(size_t index)
{
    // Clip index
    if (index > count())
        index = count();

    // Nothing to do
    if (index == head_)
        return;

    // Can't use lambda capture because operands would be of different types and could not be used
    // in the ternary operator. But closure type of lambda with no capture has non-explicit
    // conversion operator to func ptr...
    auto advance = (index < head_) ? ([](UndoStack* stk) { stk->undo_internal(); })
                                   : ([](UndoStack* stk) { stk->redo_internal(); });

    snapshot();
    while (head_ != index)
        advance(this);
    check_state_transitions();
}

void UndoStack::set_clean()
{
    snapshot();
    clean_index_ = ssize_t(head_);
    check_state_transitions();
}

void UndoStack::reset_clean()
{
    snapshot();
    reset_clean_internal();
    check_state_transitions();
}

std::string_view UndoStack::text(size_t index) const
{
    return at(index).text();
}

static constexpr std::string_view s_empty_str{""};
std::string_view UndoStack::redo_text() const
{
    return can_redo() ? history_[head_]->text() : s_empty_str;
}

std::string_view UndoStack::undo_text() const
{
    return can_undo() ? history_[head_ - 1]->text() : s_empty_str;
}

std::string UndoStack::dump() const
{
    std::stringstream ss;
    ss << "UndoStack: count=" << count() << ", head=" << head_ << ", clean_idx=" << clean_index_
       << ", clean=" << std::boolalpha << is_clean();
    return ss.str();
}

bool UndoGroup::add_stack(hash_t stack_name)
{
    // Stack name must be non-zero
    if (stack_name == 0)
        return false;

    auto result = stacks_.emplace(stack_name, UndoStack{});
    if (result.second)
    {
        // Forward stack signals to this level
        auto& stack = result.first->second;
        stack.on_head_change(on_head_change_);
        stack.on_clean_change(on_clean_change_);
        stack.on_can_undo_change(on_can_undo_change_);
        stack.on_can_redo_change(on_can_redo_change_);
        return true;
    }
    return false;
}

bool UndoGroup::remove_stack(hash_t stack_name)
{
    if (auto findit = stacks_.find(stack_name); findit != stacks_.end())
    {
        stacks_.erase(findit);
        if (active_stack_ == stack_name)
        {
            change_active_stack(0);
        }
        return true;
    }
    return false;
}

bool UndoGroup::set_active(hash_t stack_name)
{
    if (active_stack_ == stack_name)
        return true;

    if (auto findit = stacks_.find(stack_name); findit != stacks_.end())
    {
        change_active_stack(stack_name);
        return true;
    }
    return false;
}

bool UndoGroup::relabel_stack(hash_t old_name, hash_t new_name)
{
    if (auto findit = stacks_.find(old_name); findit != stacks_.end())
    {
        auto nh = stacks_.extract(old_name);
        nh.key() = new_name;
        stacks_.insert(std::move(nh));

        if (active_stack_ == old_name)
            active_stack_ = new_name;

        return true;
    }
    return false;
}

void UndoGroup::change_active_stack(hash_t stack_name)
{
    active_stack_ = stack_name;
    on_active_stack_change_(stack_name);
    on_head_change_(active_stack_ ? active_stack().head() : 0);
    on_clean_change_(active_stack_ ? active_stack().is_clean() : false);
    on_can_undo_change_(active_stack_ ? active_stack().can_undo() : false);
    on_can_redo_change_(active_stack_ ? active_stack().can_redo() : false);
}

std::string_view UndoGroup::redo_text() const
{
    return active_stack_ ? stacks_.at(active_stack_).redo_text() : s_empty_str;
}

std::string_view UndoGroup::undo_text() const
{
    return active_stack_ ? stacks_.at(active_stack_).undo_text() : s_empty_str;
}

void UndoGroup::on_head_change(std::function<void(size_t)> func)
{
    // Set functor and propagate to all stacks
    on_head_change_ = func;
    for (auto&& [name, stack] : stacks_)
        stack.on_head_change(on_head_change_);
}

void UndoGroup::on_clean_change(std::function<void(bool)> func)
{
    on_clean_change_ = func;
    for (auto&& [name, stack] : stacks_)
        stack.on_clean_change(on_clean_change_);
}

void UndoGroup::on_can_undo_change(std::function<void(bool)> func)
{
    on_can_undo_change_ = func;
    for (auto&& [name, stack] : stacks_)
        stack.on_can_undo_change(on_can_undo_change_);
}

void UndoGroup::on_can_redo_change(std::function<void(bool)> func)
{
    on_can_redo_change_ = func;
    for (auto&& [name, stack] : stacks_)
        stack.on_can_redo_change(on_can_redo_change_);
}

} // namespace undo
} // namespace kb