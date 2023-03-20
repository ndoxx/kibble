#include "undo/undo.h"

#include <iostream>

using namespace kb;
using namespace kb::undo;

struct TextBuffer
{
    std::string text;

    void dump()
    {
        std::cout << text << std::endl;
    }
};

class AppendCommand : public UndoCommand
{
public:
    AppendCommand(TextBuffer &buffer, const std::string &text)
        : UndoCommand("Append text in text buffer", 0), buffer_(buffer), text_(text)
    {
    }

    AppendCommand(TextBuffer &buffer, char c)
        : UndoCommand("Append text in text buffer", 0), buffer_(buffer), text_(1, c)
    {
    }

    void redo() override final
    {
        buffer_.text += text_;
    }

    void undo() override final
    {
        buffer_.text.erase(buffer_.text.length() - text_.length());
    }

    bool merge_with(const UndoCommand &cmd) override final
    {
        const auto &other_text = static_cast<const AppendCommand &>(cmd).text_;
        if (text_.compare(" ") != 0 && other_text.compare(" ") != 0)
        {
            text_ += other_text;
            return true;
        }
        return false;
    }

private:
    TextBuffer &buffer_;
    std::string text_;
};

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    std::cout << "Undo/Redo example" << std::endl;

    TextBuffer buf;
    UndoStack undo_stack;

    std::string typed = "hello shithead";
    for (char c : typed)
        undo_stack.push<AppendCommand>(buf, c);
    buf.dump();

    undo_stack.undo();
    buf.dump();

    typed = "world";
    for (char c : typed)
        undo_stack.push<AppendCommand>(buf, c);
    buf.dump();

    undo_stack.undo();
    buf.dump();

    undo_stack.redo();
    buf.dump();

    return 0;
}