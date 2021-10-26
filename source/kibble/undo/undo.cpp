#include "undo/undo.h"

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
        // If commands have been undone, remove all commands after head
        if (can_redo())
            history_.erase(std::next(history_.begin(), ssize_t(head_)), history_.end());

        // If limit will be exceeded, pop front command
        if ((undo_limit_ > 0) && (count() + 1 >= undo_limit_))
            history_.pop_front();

        // Execute command and push it to the stack
        cmd->redo();
        history_.push_back(std::move(cmd));
        head_ = count();
    }

    void UndoStack::clear()
    {
        history_.clear();
        head_ = 0;
    }

    void UndoStack::undo()
    {
        // Undo command under head and decrement head
        if (can_undo())
            history_[--head_]->undo();
    }

    void UndoStack::redo()
    {
        // Redo command at head and increment head
        if (can_redo())
            history_[head_++]->redo();
    }

    void UndoStack::set_head(size_t index)
    {
        if (index == head_)
            return;

        if (index > count())
            index = count();

        // Can't use lambda capture because operands would be of different types and could not be used
        // in the ternary operator. But closure type of lambda with no capture has non-explicit
        // conversion operator to func ptr... 
        auto advance = (index < head_) ? ([](UndoStack *stk)
                                          { stk->undo(); })
                                       : ([](UndoStack *stk)
                                          { stk->redo(); });
        while(head_ != index)
            advance(this);
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

} // namespace kb