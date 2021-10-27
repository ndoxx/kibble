#include "undo/undo.h"
#include <sstream>

namespace kb
{

    UndoCommand::UndoCommand(const std::string &action_text) : action_text_(action_text)
    {
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
        snapshot();
        // If commands have been undone, remove all commands after head
        if (can_redo())
        {
            history_.erase(std::next(history_.begin(), ssize_t(head_)), history_.end());
            // If clean state was located after head, reset it
            if (ssize_t(head_) < clean_index_)
                reset_clean_internal();
        }

        // If limit will be exceeded, pop front command
        if ((undo_limit_ > 0) && (count() >= undo_limit_))
            history_.pop_front();

        // Execute command and push it to the stack
        cmd->redo();
        history_.push_back(std::move(cmd));

        // Move head
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
        if (index > count())
            index = count();

        if (index == head_)
            return;

        // Can't use lambda capture because operands would be of different types and could not be used
        // in the ternary operator. But closure type of lambda with no capture has non-explicit
        // conversion operator to func ptr...
        auto advance = (index < head_) ? ([](UndoStack *stk)
                                          { stk->undo_internal(); })
                                       : ([](UndoStack *stk)
                                          { stk->redo_internal(); });

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
        ss << "UndoStack: count=" << count() << ", head=" << head_ << ", clean_idx=" << clean_index_ << ", clean=" << std::boolalpha << is_clean();
        return ss.str();
    }

} // namespace kb