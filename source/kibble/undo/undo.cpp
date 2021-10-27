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
        // If commands have been undone, remove all commands after head
        if (can_redo())
        {
            history_.erase(std::next(history_.begin(), ssize_t(head_)), history_.end());
            // If clean state was located after head, reset it
            if (ssize_t(head_) < clean_index_)
                reset_clean();
        }

        // If limit will be exceeded, pop front command
        if ((undo_limit_ > 0) && (count() >= undo_limit_))
            history_.pop_front();

        // Execute command and push it to the stack
        cmd->redo();
        history_.push_back(std::move(cmd));

        // Move head and check if the clean state was exited
        bool was_clean = is_clean();
        head_ = count();
        on_head_change_(head());
        check_clean_changed(was_clean);
    }

    void UndoStack::clear()
    {
        history_.clear();
        head_ = 0;
        on_head_change_(head());
        reset_clean();
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

    void UndoStack::check_clean_changed(bool was_clean)
    {
        if (is_clean() != was_clean)
            on_clean_change_(is_clean());
    }

    void UndoStack::undo()
    {
        if (can_undo())
        {
            bool was_clean = is_clean();
            undo_internal();
            on_head_change_(head());
            check_clean_changed(was_clean);
        }
    }
    void UndoStack::redo()
    {
        if (can_redo())
        {
            bool was_clean = is_clean();
            redo_internal();
            on_head_change_(head());
            check_clean_changed(was_clean);
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
        bool was_clean = is_clean();
        while (head_ != index)
            advance(this);

        on_head_change_(head());
        check_clean_changed(was_clean);
    }

    void UndoStack::set_clean()
    {
        bool was_clean = is_clean();
        clean_index_ = ssize_t(head_);
        check_clean_changed(was_clean);
    }

    void UndoStack::reset_clean()
    {
        bool was_clean = is_clean();
        clean_index_ = -1;
        check_clean_changed(was_clean);
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