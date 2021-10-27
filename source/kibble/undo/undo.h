#pragma once

#include <string>
#include <memory>
#include <string_view>
#include <deque>
#include <vector>
#include <functional>

/*
TODO:
    [ ] Macro commands
        -> UndoCommands can register a parent on construction
        -> UndoCommand::redo() default impl calls all children redo() in order
        -> UndoCommand::undo() default impl calls all children undo() in reverse order
    [ ] Undo groups
*/

namespace kb
{

    class UndoCommand
    {
    public:
        friend class MacroCommandBuilder;

        explicit UndoCommand(const std::string &action_text, ssize_t merge_id = -1);
        UndoCommand(const std::string &action_text, std::vector<std::unique_ptr<UndoCommand>> children, ssize_t merge_id = -1);
        virtual ~UndoCommand() = default;

        inline const std::string &text() const { return action_text_; }
        inline ssize_t merge_id() const { return merge_id_; }
        inline void set_obsolete() { obsolete_ = true; }
        inline bool is_obsolete() const { return obsolete_; }

        inline size_t child_count() const { return children_.size(); }
        inline const UndoCommand &child(size_t index) { return *children_.at(index); }

        virtual void undo();
        virtual void redo();
        virtual bool merge_with(const UndoCommand &cmd);

    protected:
        ssize_t merge_id_;

    private:
        bool obsolete_ = false;
        std::string action_text_;
        std::vector<std::unique_ptr<UndoCommand>> children_;
    };

    class MacroCommandBuilder
    {
    public:
        explicit MacroCommandBuilder(const std::string &action_text, ssize_t merge_id = -1);
        void push(std::unique_ptr<UndoCommand> cmd);

        template <typename C, typename... Args>
        inline void push(Args &&...args)
        {
            push(std::make_unique<C>(std::forward<Args>(args)...));
        }

        std::unique_ptr<UndoCommand> build();

    private:
        ssize_t merge_id_;
        std::string action_text_;
        std::vector<std::unique_ptr<UndoCommand>> children_;
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
        inline void on_can_undo_change(std::function<void(bool)> func) { on_can_undo_change_ = func; }
        inline void on_can_redo_change(std::function<void(bool)> func) { on_can_redo_change_ = func; }

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

        void snapshot();
        void check_state_transitions();
        void undo_internal();
        void redo_internal();
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

} // namespace kb
