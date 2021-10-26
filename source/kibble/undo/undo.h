#pragma once

#include <string>
#include <memory>
#include <string_view>
#include <deque>
#include <functional>

/*
TODO:
    [ ] Macro commands
        -> UndoCommands can register a parent on construction
        -> UndoCommand::redo() default impl calls all children redo() in order
        -> UndoCommand::undo() default impl calls all children undo() in reverse order
    [ ] Clean state
        -> On save (for example) the stack is marked as clean
        -> The clean state can be reached back after undoing / redoing actions
        -> This keeps track of the need to save a document (often marked by a * in editors title bar)
    [ ] Event system coupling
        -> on_clean_change
        -> on_can_redo_change
        -> on_can_undo_change
*/

namespace kb
{

    class UndoCommand
    {
    public:
        UndoCommand(const std::string &action_text);
        virtual ~UndoCommand() = default;

        inline const std::string &text() const { return action_text_; }

        virtual void undo() = 0;
        virtual void redo() = 0;

    private:
        std::string action_text_;
    };

    class UndoStack
    {
    public:
        bool set_undo_limit(size_t undo_limit);
        void push(std::unique_ptr<UndoCommand> cmd);

        template <typename C, typename... Args>
        inline void push(Args &&...args)
        {
            push(std::make_unique<C>(std::forward<Args>(args)...));
        }

        void clear();
        void set_head(size_t index);

        std::string_view text(size_t index) const; // string_view because it may be empty
        std::string_view redo_text() const;
        std::string_view undo_text() const;

        inline const UndoCommand &at(size_t index) const { return *history_.at(index); }
        inline size_t count() const { return history_.size(); }
        inline size_t head() const { return head_; }
        inline size_t limit() const { return undo_limit_; }
        inline bool empty() const { return history_.empty(); }
        inline bool can_redo() const { return head_ < count(); }
        inline bool can_undo() const { return head_ > 0; }

        inline void on_head_change(std::function<void(size_t)> func) { on_head_change_ = func; }

    private:
        void undo_internal();
        void redo_internal();

    public:
        inline void undo()
        {
            if (can_undo())
            {
                undo_internal();
                on_head_change_(head());
            }
        }
        inline void redo()
        {
            if (can_redo())
            {
                redo_internal();
                on_head_change_(head());
            }
        }

    private:
        std::deque<std::unique_ptr<UndoCommand>> history_;
        size_t undo_limit_ = 0;
        size_t head_ = 0;

        std::function<void(size_t)> on_head_change_ = [](size_t) {};
    };

} // namespace kb
