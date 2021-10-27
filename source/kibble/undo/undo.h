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
    [ ] Event system coupling
        -> on_can_redo_change
        -> on_can_undo_change
    [ ] Commands merging
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
        void undo();
        void redo();
        void set_head(size_t index);
        void set_clean();
        void reset_clean();

        std::string_view text(size_t index) const; // string_view because it may be empty
        std::string_view redo_text() const;
        std::string_view undo_text() const;

        inline const UndoCommand &at(size_t index) const { return *history_.at(index); }
        inline size_t count() const { return history_.size(); }
        inline size_t head() const { return head_; }
        inline size_t limit() const { return undo_limit_; }
        inline ssize_t clean_index() const { return clean_index_; }
        inline bool empty() const { return history_.empty(); }
        inline bool can_redo() const { return head_ < count(); }
        inline bool can_undo() const { return head_ > 0; }
        inline bool is_clean() const { return ssize_t(head_) == clean_index_; }

        inline void on_head_change(std::function<void(size_t)> func) { on_head_change_ = func; }
        inline void on_clean_change(std::function<void(bool)> func) { on_clean_change_ = func; }

        std::string dump() const;

    private:
        void undo_internal();
        void redo_internal();
        void check_clean_changed(bool was_clean);

    private:
        std::deque<std::unique_ptr<UndoCommand>> history_;
        size_t undo_limit_ = 0;
        size_t head_ = 0;
        ssize_t clean_index_ = -1;

        std::function<void(size_t)> on_head_change_ = [](size_t) {};
        std::function<void(bool)> on_clean_change_ = [](bool) {};
    };

} // namespace kb
