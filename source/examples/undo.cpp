#include "logger/dispatcher.h"
#include "logger/logger.h"
#include "logger/sink.h"
#include "undo/undo.h"

using namespace kb;

struct GameObject
{
    int uuid = 0;
    int position = 0;
    bool alive = true;
};

class GOMoveUndoCommand : public kb::UndoCommand
{
public:
    GOMoveUndoCommand(GameObject *go, int increment) : kb::UndoCommand("Change game object position"),
                                                       go_(go),
                                                       increment_(increment)

    {
    }

    void redo() override
    {
        KLOGN("undo") << "Move::redo" << std::endl;
        KLOG("scene", 1) << "Object " << go_->uuid << " moved from " << go_->position;
        go_->position += increment_;
        KLOG("scene", 1) << " to " << go_->position << std::endl;
    }

    void undo() override
    {
        KLOGN("undo") << "Move::undo" << std::endl;
        KLOG("scene", 1) << "Object " << go_->uuid << " moved from " << go_->position;
        go_->position -= increment_;
        KLOG("scene", 1) << " to " << go_->position << std::endl;
    }

private:
    GameObject *go_ = nullptr;
    int increment_ = 0;
};

class GOKillUndoCommand : public kb::UndoCommand
{
public:
    GOKillUndoCommand(GameObject *go) : kb::UndoCommand("Kill game object"),
                                        go_(go)

    {
    }

    void redo() override
    {
        KLOGN("undo") << "Kill::redo" << std::endl;
        go_->alive = false;
        KLOG("scene", 1) << "Object " << go_->uuid << " was killed" << std::endl;
    }

    void undo() override
    {
        KLOGN("undo") << "Kill::undo" << std::endl;
        go_->alive = true;
        KLOG("scene", 1) << "Object " << go_->uuid << " was resurrected" << std::endl;
    }

private:
    GameObject *go_ = nullptr;
};

void init_logger()
{
    KLOGGER_START();

    KLOGGER(create_channel("nuclear", 3));
    KLOGGER(create_channel("undo", 3));
    KLOGGER(create_channel("scene", 3));
    KLOGGER(attach_all("console_sink", std::make_unique<klog::ConsoleSink>()));
    KLOGGER(set_backtrace_on_error(false));
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    init_logger();

    KLOGN("nuclear") << "Undo/Redo example" << std::endl;

    GameObject go0{42, 8, true};
    UndoStack undo_stack;

    auto print_state = [&go0, &undo_stack]()
    {
        KLOG("undo", 1) << "State: " << std::endl;
        KLOGI << "game object: {pos=" << go0.position << ", alive=" << std::boolalpha << go0.alive << '}' << std::endl;
        KLOGI << "undo stack:  {head=" << undo_stack.head() << ", count=" << undo_stack.count() << '}' << std::endl;
    };

    print_state();
    undo_stack.push<GOMoveUndoCommand>(&go0, 5);
    print_state();
    undo_stack.undo();
    print_state();
    undo_stack.redo();
    print_state();
    undo_stack.push<GOMoveUndoCommand>(&go0, 2);
    print_state();
    undo_stack.push<GOMoveUndoCommand>(&go0, -4);
    print_state();
    undo_stack.push<GOKillUndoCommand>(&go0);
    print_state();
    undo_stack.undo();
    print_state();
    undo_stack.undo();
    print_state();
    undo_stack.push<GOMoveUndoCommand>(&go0, -15);
    print_state();
    undo_stack.set_head(1);
    print_state();

    return 0;
}