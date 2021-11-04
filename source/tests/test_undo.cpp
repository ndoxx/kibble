#include "undo/undo.h"
#include <catch2/catch_all.hpp>
#include <iostream>
#include <numeric>

struct GameObject
{
    int uuid = 0;
    int position = 0;
    bool alive = true;

    bool operator==(const GameObject &other) const
    {
        return uuid == other.uuid && position == other.position && alive == other.alive;
    }
};

class GOMoveUndoCommand : public kb::undo::UndoCommand
{
public:
    GOMoveUndoCommand(GameObject *go, int increment)
        : kb::undo::UndoCommand("Change game object position"), go_(go), increment_(increment)

    {
    }

    void redo() override
    {
        go_->position += increment_;
    }

    void undo() override
    {
        go_->position -= increment_;
    }

private:
    GameObject *go_ = nullptr;
    int increment_ = 0;
};

class GOKillUndoCommand : public kb::undo::UndoCommand
{
public:
    GOKillUndoCommand(GameObject *go) : kb::undo::UndoCommand("Kill game object"), go_(go)

    {
    }

    void redo() override
    {
        go_->alive = false;
    }

    void undo() override
    {
        go_->alive = true;
    }

private:
    GameObject *go_ = nullptr;
};

class UndoRedoFixture
{
public:
    UndoRedoFixture() : go_{42, 0, true}
    {
        undo_stack_.on_head_change([this](size_t new_head) { new_head_ = new_head; });
        undo_stack_.on_can_undo_change([this](bool can_undo) { new_can_undo_ = can_undo; });
        undo_stack_.on_can_redo_change([this](bool can_redo) { new_can_redo_ = can_redo; });
    }

protected:
    struct Snapshot
    {
        GameObject go_state;
        size_t stack_head;
        size_t stack_count;

        bool operator==(const Snapshot &other) const
        {
            return go_state == other.go_state && stack_head == other.stack_head && stack_count == other.stack_count;
        }
    };

    Snapshot snap()
    {
        return Snapshot{go_, undo_stack_.head(), undo_stack_.count()};
    }

protected:
    GameObject go_;
    kb::undo::UndoStack undo_stack_;

    size_t new_head_ = std::numeric_limits<size_t>::max();
    bool new_can_undo_ = false;
    bool new_can_redo_ = false;
};

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command should execute it", "[push]")
{
    const int increment = 5;
    auto old_state = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, increment);
    auto new_state = snap();

    REQUIRE(new_state.go_state.position == old_state.go_state.position + increment);
}

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command should increment stack count and set head to count", "[push]")
{
    const int increment = 5;
    auto old_state = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, increment);
    auto new_state = snap();

    REQUIRE(new_state.stack_count == old_state.stack_count + 1);
    REQUIRE(new_state.stack_head == new_state.stack_count);
    // Check that on_head_change functor was called
    REQUIRE(new_head_ == new_state.stack_head);
}

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command should make it undoable", "[push]")
{
    REQUIRE_FALSE(undo_stack_.can_undo());
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    REQUIRE(undo_stack_.can_undo());
    REQUIRE(new_can_undo_);
}

TEST_CASE_METHOD(UndoRedoFixture, "undoing a command should make it redoable", "[undo]")
{
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    REQUIRE_FALSE(undo_stack_.can_redo());
    REQUIRE_FALSE(new_can_redo_);
    undo_stack_.undo();
    REQUIRE(undo_stack_.can_redo());
    REQUIRE(new_can_redo_);
}

TEST_CASE_METHOD(UndoRedoFixture, "undoing a command rolls back the state and moves head", "[undo]")
{
    const int increment = 5;
    auto initial_state = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, increment);
    auto old_state = snap();
    undo_stack_.undo();
    auto new_state = snap();

    REQUIRE(new_state.go_state == initial_state.go_state);
    REQUIRE(new_state.stack_count == old_state.stack_count);
    REQUIRE(new_state.stack_head == old_state.stack_head - 1);
    REQUIRE(new_head_ == new_state.stack_head);
    REQUIRE_FALSE(new_can_undo_);
}

TEST_CASE_METHOD(UndoRedoFixture, "undoing on an empty stack does nothing", "[undo]")
{
    REQUIRE_FALSE(undo_stack_.can_undo());

    auto old_state = snap();
    undo_stack_.undo();
    auto new_state = snap();

    REQUIRE(new_state == old_state);
    REQUIRE(new_head_ == std::numeric_limits<size_t>::max());
}

TEST_CASE_METHOD(UndoRedoFixture, "redoing a command reexecutes it and moves head", "[redo]")
{
    const int increment = 5;
    undo_stack_.push<GOMoveUndoCommand>(&go_, increment);
    auto state_1 = snap();
    undo_stack_.undo();
    auto state_2 = snap();
    undo_stack_.redo();
    auto state_3 = snap();

    REQUIRE(state_3.go_state == state_1.go_state);
    REQUIRE(state_3.stack_count == state_2.stack_count);
    REQUIRE(state_3.stack_head == state_2.stack_head + 1);
    REQUIRE(new_head_ == state_3.stack_head);
}

TEST_CASE_METHOD(UndoRedoFixture, "redoing when head is at count does nothing", "[redo]")
{
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 2);

    REQUIRE_FALSE(undo_stack_.can_redo());

    auto old_state = snap();
    undo_stack_.redo();
    auto new_state = snap();

    REQUIRE(new_state == old_state);
}

TEST_CASE_METHOD(UndoRedoFixture, "setting head before its current position should undo iteratively", "[sethead]")
{
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    auto state_1 = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 2);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 3);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 4);

    undo_stack_.set_head(1);
    auto state_2 = snap();

    REQUIRE(state_1.go_state == state_2.go_state);
    REQUIRE(state_2.stack_head == 1);
    REQUIRE(new_head_ == state_2.stack_head);
}

TEST_CASE_METHOD(UndoRedoFixture, "setting head after its current position should redo iteratively", "[sethead]")
{
    for (size_t ii = 0; ii < 4; ++ii)
        undo_stack_.push<GOMoveUndoCommand>(&go_, ii + 1);

    auto state_1 = snap();
    for (size_t ii = 0; ii < 4; ++ii)
        undo_stack_.undo();

    undo_stack_.set_head(4);
    auto state_2 = snap();

    REQUIRE(state_1 == state_2);
    REQUIRE(new_head_ == state_2.stack_head);
}

TEST_CASE_METHOD(UndoRedoFixture, "setting head after count should only set it to count", "[sethead]")
{
    for (size_t ii = 0; ii < 4; ++ii)
        undo_stack_.push<GOMoveUndoCommand>(&go_, ii + 1);

    auto state_1 = snap();
    for (size_t ii = 0; ii < 4; ++ii)
        undo_stack_.undo();

    undo_stack_.set_head(42);
    auto state_2 = snap();

    REQUIRE(state_1 == state_2);
}

TEST_CASE_METHOD(UndoRedoFixture, "setting head on an empty stack does nothing", "[sethead]")
{
    auto state_1 = snap();
    undo_stack_.set_head(42);
    auto state_2 = snap();

    REQUIRE(state_1 == state_2);
    REQUIRE(new_head_ == std::numeric_limits<size_t>::max());
}

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command should clear redoable commands in stack", "[push]")
{
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 2);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 3);
    undo_stack_.undo();
    undo_stack_.undo();
    auto old_state = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 4);
    auto new_state = snap();

    REQUIRE(new_state.stack_count == old_state.stack_count - 2 + 1); // cleared two commands, added one
    REQUIRE(new_state.stack_head == new_state.stack_count);
    REQUIRE(new_state.go_state.position == old_state.go_state.position + 4);
}

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command in a full stack should pop first command", "[limit]")
{
    constexpr size_t undo_limit = 3;
    bool success = undo_stack_.set_undo_limit(undo_limit);
    REQUIRE(success);

    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    auto state_1 = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 2);
    undo_stack_.push<GOMoveUndoCommand>(&go_, 3);
    auto state_2 = snap();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 4);
    auto state_3 = snap();

    REQUIRE(state_3.stack_count == undo_limit);
    REQUIRE(state_3.stack_count == state_2.stack_count);

    // Can only roll back so far
    for (size_t ii = 0; ii < undo_limit + 1; ++ii)
        undo_stack_.undo();
    auto state_4 = snap();

    REQUIRE(state_4.go_state == state_1.go_state);
}

TEST_CASE_METHOD(UndoRedoFixture, "setting the undo limit on a non empty stack should fail and do nothing", "[limit]")
{
    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    bool success = undo_stack_.set_undo_limit(42);
    REQUIRE_FALSE(success);
    REQUIRE(undo_stack_.limit() == 0);
}

TEST_CASE_METHOD(UndoRedoFixture, "clearing stack should reset head and count to 0", "[clear]")
{
    for (size_t ii = 0; ii < 8; ++ii)
        undo_stack_.push<GOMoveUndoCommand>(&go_, ii);

    undo_stack_.clear();
    REQUIRE(undo_stack_.head() == 0);
    REQUIRE(undo_stack_.count() == 0);
    REQUIRE(undo_stack_.empty());
}

TEST_CASE_METHOD(UndoRedoFixture, "pushing a command should change the undo text", "[push]")
{
    using std::operator""sv;

    auto undo_text = undo_stack_.undo_text();
    REQUIRE(undo_text.compare(""sv) == 0);

    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    undo_text = undo_stack_.undo_text();
    REQUIRE(undo_text.compare("Change game object position"sv) == 0);

    undo_stack_.push<GOKillUndoCommand>(&go_);
    undo_text = undo_stack_.undo_text();
    REQUIRE(undo_text.compare("Kill game object"sv) == 0);
}

TEST_CASE_METHOD(UndoRedoFixture, "undoing a command should change the redo text", "[undo]")
{
    using std::operator""sv;

    undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
    undo_stack_.push<GOKillUndoCommand>(&go_);

    auto redo_text = undo_stack_.redo_text();
    REQUIRE(redo_text.compare(""sv) == 0);

    undo_stack_.undo();
    redo_text = undo_stack_.redo_text();
    REQUIRE(redo_text.compare("Kill game object"sv) == 0);

    undo_stack_.undo();
    redo_text = undo_stack_.redo_text();
    REQUIRE(redo_text.compare("Change game object position"sv) == 0);
}

class CleanStateFixture
{
public:
    CleanStateFixture() : go_{42, 0, true}
    {
        undo_stack_.on_clean_change([this](bool is_clean) {
            new_is_clean_ = is_clean;
            ++clean_transitions_;
        });
        // Push a few commands
        undo_stack_.push<GOMoveUndoCommand>(&go_, 1);
        undo_stack_.push<GOMoveUndoCommand>(&go_, 2);
        undo_stack_.push<GOMoveUndoCommand>(&go_, 4);
        undo_stack_.push<GOMoveUndoCommand>(&go_, 8);
        undo_stack_.undo();
    }

protected:
    GameObject go_;
    kb::undo::UndoStack undo_stack_;

    bool new_is_clean_ = false;
    size_t clean_transitions_ = 0;
};

TEST_CASE_METHOD(CleanStateFixture, "Setting clean state should transition the clean state", "[clean]")
{
    undo_stack_.set_clean();
    REQUIRE(undo_stack_.is_clean());
    REQUIRE(new_is_clean_);
    REQUIRE(clean_transitions_ == 1);
}

TEST_CASE_METHOD(CleanStateFixture, "Setting clean state should set the clean index at head", "[clean]")
{
    undo_stack_.set_clean();
    REQUIRE(undo_stack_.clean_index() == ssize_t(undo_stack_.head()));
}

TEST_CASE_METHOD(CleanStateFixture, "Clearing the stack should reset the clean index", "[clean]")
{
    undo_stack_.clear();
    REQUIRE(undo_stack_.clean_index() == -1);
}

TEST_CASE_METHOD(CleanStateFixture, "Resetting clean state should work", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.reset_clean();
    REQUIRE_FALSE(new_is_clean_);
    REQUIRE(clean_transitions_ == 2);
}

TEST_CASE_METHOD(CleanStateFixture, "Pushing on clean state should exit the clean state", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 16);
    REQUIRE_FALSE(new_is_clean_);
    REQUIRE(clean_transitions_ == 2);
}

TEST_CASE_METHOD(CleanStateFixture, "Undoing on clean state should exit the clean state", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.undo();
    REQUIRE_FALSE(new_is_clean_);
    REQUIRE(clean_transitions_ == 2);
}

TEST_CASE_METHOD(CleanStateFixture, "Pushing before clean state should reset the clean index", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.undo();
    undo_stack_.undo();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 16);
    REQUIRE(undo_stack_.clean_index() == -1);
}

TEST_CASE_METHOD(CleanStateFixture, "Setting head before clean state should exit the clean state", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.set_head(0);
    REQUIRE_FALSE(new_is_clean_);
    REQUIRE(clean_transitions_ == 2);
}

TEST_CASE_METHOD(CleanStateFixture, "Setting head after clean state should exit the clean state", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.set_head(undo_stack_.count());
    REQUIRE_FALSE(new_is_clean_);
    REQUIRE(clean_transitions_ == 2);
}

TEST_CASE_METHOD(CleanStateFixture, "Clean state can be reached back using undo", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.push<GOMoveUndoCommand>(&go_, 32);
    undo_stack_.undo();
    REQUIRE(new_is_clean_);
    REQUIRE(clean_transitions_ == 3);
}

TEST_CASE_METHOD(CleanStateFixture, "Clean state can be reached back using redo", "[clean]")
{
    undo_stack_.set_clean();
    undo_stack_.undo();
    REQUIRE_FALSE(new_is_clean_);
    undo_stack_.redo();
    REQUIRE(new_is_clean_);
    REQUIRE(clean_transitions_ == 3);
}

TEST_CASE_METHOD(CleanStateFixture, "Clean state can be reached back using set_head", "[clean]")
{
    undo_stack_.set_clean();
    size_t clean_index = size_t(undo_stack_.clean_index());
    undo_stack_.set_head(0);
    undo_stack_.set_head(clean_index);
    REQUIRE(new_is_clean_);
    REQUIRE(clean_transitions_ == 3);
}

struct Orientable
{
    float angle = 0.f;
};

class RotateCommand : public kb::undo::UndoCommand
{
public:
    RotateCommand(Orientable *obj, float increment)
        : kb::undo::UndoCommand("Change orientable object angle", 0), obj_(obj), increment_(increment)

    {
    }

    void redo() override
    {
        obj_->angle += increment_;
    }

    void undo() override
    {
        obj_->angle -= increment_;
    }

    static constexpr float EPSILON = 1e-5f;
    bool merge_with(const UndoCommand &cmd) override final
    {
        increment_ += static_cast<const RotateCommand &>(cmd).increment_;
        if (std::fabs(increment_) < EPSILON)
            set_obsolete();
        return true;
    }

    inline float get_increment() const
    {
        return increment_;
    }

private:
    Orientable *obj_ = nullptr;
    float increment_ = 0.f;
};

class SetAngleCommand : public kb::undo::UndoCommand
{
public:
    SetAngleCommand(Orientable *obj, float value)
        : kb::undo::UndoCommand("Set orientable object angle"), obj_(obj), value_(value), old_value_(obj_->angle)

    {
    }

    void redo() override
    {
        obj_->angle = value_;
    }

    void undo() override
    {
        obj_->angle = old_value_;
    }

    inline float get_value() const
    {
        return value_;
    }

private:
    Orientable *obj_ = nullptr;
    float value_ = 0.f;
    float old_value_ = 0.f;
};

class MergeFixture
{
public:
    MergeFixture() : obj_{0.f}
    {
    }

    void dump()
    {
        std::cout << undo_stack_.dump() << std::endl;
    }

protected:
    Orientable obj_;
    kb::undo::UndoStack undo_stack_;
};

TEST_CASE_METHOD(MergeFixture, "pushing two compatible commands should merge them", "[merge]")
{
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    REQUIRE(undo_stack_.count() == 1);
    REQUIRE(static_cast<const RotateCommand &>(undo_stack_.at(0)).get_increment() == 2.f);
}

TEST_CASE_METHOD(MergeFixture, "merged commands should behave atomically with respect to undo", "[merge]")
{
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.undo();
    REQUIRE(undo_stack_.count() == 1);
    REQUIRE(undo_stack_.head() == 0);
}

TEST_CASE_METHOD(MergeFixture, "merged commands should behave atomically with respect to redo", "[merge]")
{
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.undo();
    undo_stack_.redo();
    REQUIRE(undo_stack_.head() == 1);
}

TEST_CASE_METHOD(MergeFixture, "pushing two compatible commands after an undo should still erase commands after head",
                 "[merge]")
{
    undo_stack_.push<SetAngleCommand>(&obj_, 8.f);
    undo_stack_.undo();
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    REQUIRE(undo_stack_.count() == 1);
    REQUIRE(static_cast<const RotateCommand &>(undo_stack_.at(0)).get_increment() == 2.f);
}

TEST_CASE_METHOD(MergeFixture, "incompatible commands should not interfere", "[merge]")
{
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<SetAngleCommand>(&obj_, 8.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    REQUIRE(undo_stack_.count() == 3);
}

TEST_CASE_METHOD(MergeFixture, "commands that merge to an obsolete command should be deleted", "[merge]")
{
    undo_stack_.push<SetAngleCommand>(&obj_, 8.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, -1.f);
    REQUIRE(undo_stack_.count() == 1);
    REQUIRE(static_cast<const SetAngleCommand &>(undo_stack_.at(0)).get_value() == 8.f);
}

TEST_CASE_METHOD(MergeFixture, "clean state can be reached back after deletion of an obsolete command", "[merge]")
{
    undo_stack_.push<SetAngleCommand>(&obj_, 8.f);
    undo_stack_.set_clean();
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.push<RotateCommand>(&obj_, -1.f);
    REQUIRE(undo_stack_.is_clean());
}

TEST_CASE_METHOD(MergeFixture,
                 "clean index should be reset if it is greater than or equal to the index of an obsolete command",
                 "[merge]")
{
    undo_stack_.push<SetAngleCommand>(&obj_, 8.f);
    undo_stack_.push<RotateCommand>(&obj_, 1.f);
    undo_stack_.set_clean();
    undo_stack_.push<RotateCommand>(&obj_, -1.f);
    REQUIRE(undo_stack_.clean_index() == -1);
}

struct Account
{
    float balance = 0.f;
    std::string owner;
};

struct Operation
{
    size_t idx = 0;
    float value = 0.f;
};

class Bank
{
public:
    Bank() : accounts({{100.f, "Alice"}, {1234.5f, "Bob"}, {549862.23f, "Carol"}, {23649.1f, "Danny"}})
    {
    }

    std::vector<Account> accounts;
    std::vector<Operation> journal;
};

class Deposit : public kb::undo::UndoCommand
{
public:
    Deposit(Bank &bnk, size_t idx, uint32_t amt_cents)
        : kb::undo::UndoCommand("Deposit amt_cents into idx's account"), bnk_(bnk), idx_(idx),
          value_(float(amt_cents) / 100.f)

    {
    }

    void redo() override
    {
        bnk_.accounts[idx_].balance += value_;
        bnk_.journal.push_back({idx_, value_});
    }
    void undo() override
    {
        bnk_.accounts[idx_].balance -= value_;
        bnk_.journal.push_back({idx_, -value_});
    }
    inline float get_value() const
    {
        return value_;
    }

private:
    Bank &bnk_;
    size_t idx_ = 0;
    float value_ = 0.f;
};

class Withdraw : public kb::undo::UndoCommand
{
public:
    Withdraw(Bank &bnk, size_t idx, uint32_t amt_cents)
        : kb::undo::UndoCommand("Withdraw amt_cents into idx's account"), bnk_(bnk), idx_(idx),
          value_(float(amt_cents) / 100.f)

    {
    }

    void redo() override
    {
        bnk_.accounts[idx_].balance -= value_;
        bnk_.journal.push_back({idx_, -value_});
    }
    void undo() override
    {
        bnk_.accounts[idx_].balance += value_;
        bnk_.journal.push_back({idx_, value_});
    }
    inline float get_value() const
    {
        return value_;
    }

private:
    Bank &bnk_;
    size_t idx_ = 0;
    float value_ = 0.f;
};

class MacroFixture
{
public:
    MacroFixture()
    {
        // Push some commands
        undo_stack_.push<Deposit>(bnk_, 0, 20000);
        undo_stack_.push<Withdraw>(bnk_, 1, 45600);
    }

    void transfer(size_t source, size_t destination, uint32_t amt_cents)
    {
        auto trans = std::make_unique<kb::undo::UndoCommand>("Transfer an amount between accounts");
        trans->push<Withdraw>(bnk_, source, amt_cents);
        trans->push<Deposit>(bnk_, destination, amt_cents);
        undo_stack_.push(std::move(trans));
    }

    auto snap()
    {
        return bnk_.accounts;
    }

    void dump()
    {
        std::cout << undo_stack_.dump() << std::endl;
    }

protected:
    Bank bnk_;
    kb::undo::UndoStack undo_stack_;
};

TEST_CASE_METHOD(MacroFixture, "A macro should count as a single command", "[macro]")
{
    size_t count = undo_stack_.count();
    transfer(0, 1, 20000);
    REQUIRE(undo_stack_.count() == count + 1);
}

TEST_CASE_METHOD(MacroFixture, "Pushing/redoing a macro should execute all its children in order", "[macro]")
{
    uint32_t amt = 20000;
    auto before = snap();
    transfer(0, 1, amt);
    auto after = snap();
    REQUIRE(after[0].balance == before[0].balance - float(amt) / 100.f);
    REQUIRE(after[1].balance == before[1].balance + float(amt) / 100.f);

    size_t last_op_idx = bnk_.journal.size() - 1;
    REQUIRE(bnk_.journal[last_op_idx - 1].idx == 0);
    REQUIRE(bnk_.journal[last_op_idx].idx == 1);
}

TEST_CASE_METHOD(MacroFixture, "Undoing a macro should undo all its children in reverse order", "[macro]")
{
    uint32_t amt = 20000;
    transfer(0, 1, amt);
    auto before = snap();
    undo_stack_.undo();
    auto after = snap();
    REQUIRE(after[0].balance == before[0].balance + float(amt) / 100.f);
    REQUIRE(after[1].balance == before[1].balance - float(amt) / 100.f);

    size_t last_op_idx = bnk_.journal.size() - 1;
    REQUIRE(bnk_.journal[last_op_idx - 1].idx == 1);
    REQUIRE(bnk_.journal[last_op_idx].idx == 0);
}

using hash_t = kb::hash_t;
struct TextBuffer
{
    std::string text;
    hash_t name;
};

class AppendCommand : public kb::undo::UndoCommand
{
public:
    AppendCommand(TextBuffer &buffer, const std::string &text)
        : kb::undo::UndoCommand("Append text in text buffer", 0), buffer_(buffer), text_(text)
    {
    }

    AppendCommand(TextBuffer &buffer, char c)
        : kb::undo::UndoCommand("Append text in text buffer", 0), buffer_(buffer), text_(1, c)
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

class GroupFixture
{
public:
    struct State
    {
        hash_t active_stack = 0;
        size_t head = 0;
        bool clean = false;
        bool can_undo = false;
        bool can_redo = false;
    };

    GroupFixture()
    {
        group_.on_active_stack_change([this](hash_t active_stack) { last_state_.active_stack = active_stack; });
        group_.on_head_change([this](size_t head) { last_state_.head = head; });
        group_.on_clean_change([this](bool clean) { last_state_.clean = clean; });
        group_.on_can_undo_change([this](bool can_undo) { last_state_.can_undo = can_undo; });
        group_.on_can_redo_change([this](bool can_redo) { last_state_.can_redo = can_redo; });

        create_doc("doc0"_h);
        create_doc("doc1"_h);
        create_doc("doc2"_h);
    }

    void focus(hash_t name)
    {
        auto findit = names_.find(name);
        size_t idx = findit->second;
        current_buf_ = idx;
        current_doc_ = name;
        group_.set_active(current_doc_);
    }

    void append(const std::string &text)
    {
        group_.push<AppendCommand>(bufs_[current_buf_], text);
    }

    void append(char c)
    {
        group_.push<AppendCommand>(bufs_[current_buf_], c);
    }

    void create_doc(hash_t name)
    {
        bufs_.push_back(TextBuffer{"", name});
        group_.add_stack(name);
        names_.insert({name, group_.size() - 1});
    }

    void destroy_doc(hash_t name)
    {
        auto findit = names_.find(name);
        size_t idx = findit->second;
        bufs_.erase(bufs_.begin() + long(idx));
        group_.remove_stack(name);
        // reindex
        names_.clear();
        for (size_t ii = 0; ii < bufs_.size(); ++ii)
            names_.insert({bufs_[ii].name, ii});
    }

protected:
    std::vector<TextBuffer> bufs_;
    kb::undo::UndoGroup group_;
    State last_state_;
    hash_t current_doc_;
    size_t current_buf_;
    std::unordered_map<hash_t, size_t> names_;
};

TEST_CASE_METHOD(GroupFixture, "If no active stack is selected, state cannot change, and getting active stack throws",
                 "[group]")
{
    current_buf_ = 0;
    current_doc_ = "doc0"_h;

    append('h');
    REQUIRE(last_state_.head == 0);

    group_.undo();
    REQUIRE(last_state_.head == 0);

    group_.set_clean();
    REQUIRE_FALSE(last_state_.clean);

    REQUIRE_THROWS(group_.active_stack());
}

TEST_CASE_METHOD(GroupFixture, "Selecting an active stack should cause a state transition", "[group]")
{
    focus("doc0"_h);
    REQUIRE(last_state_.active_stack == "doc0"_h);
}

TEST_CASE_METHOD(GroupFixture, "Group operations are forwarded to the active stack only", "[group]")
{
    focus("doc0"_h);
    group_.set_clean();
    append('h');
    REQUIRE(group_.stack("doc0"_h).count() == 1);
    REQUIRE(group_.stack("doc1"_h).count() == 0);
    REQUIRE(group_.stack("doc2"_h).count() == 0);
    REQUIRE_FALSE(group_.stack("doc0"_h).is_clean());
    REQUIRE_FALSE(group_.stack("doc1"_h).is_clean());
    REQUIRE_FALSE(group_.stack("doc2"_h).is_clean());
    REQUIRE(group_.stack("doc0"_h).can_undo());
    REQUIRE_FALSE(group_.stack("doc1"_h).can_undo());
    REQUIRE_FALSE(group_.stack("doc2"_h).can_undo());

    group_.undo();
    REQUIRE(group_.stack("doc0"_h).head() == 0);
    REQUIRE(group_.stack("doc0"_h).is_clean());
    REQUIRE_FALSE(group_.stack("doc0"_h).can_undo());
    REQUIRE(group_.stack("doc0"_h).can_redo());
    REQUIRE_FALSE(group_.stack("doc1"_h).can_redo());
    REQUIRE_FALSE(group_.stack("doc2"_h).can_redo());

    group_.redo();
    REQUIRE(group_.stack("doc0"_h).head() == 1);
    REQUIRE(group_.stack("doc1"_h).head() == 0);
    REQUIRE(group_.stack("doc2"_h).head() == 0);
}

TEST_CASE_METHOD(GroupFixture, "On adding a new stack, functors should work as expected", "[group]")
{
    focus("doc0"_h);
    create_doc("doc3"_h);
    REQUIRE(group_.size() == 4);

    focus("doc3"_h);
    group_.set_clean();
    append('h');
    REQUIRE(last_state_.head == 1);
    REQUIRE(last_state_.can_undo);
    REQUIRE_FALSE(last_state_.can_redo);
    REQUIRE_FALSE(last_state_.clean);

    group_.undo();
    REQUIRE(last_state_.can_redo);
}

TEST_CASE_METHOD(GroupFixture, "Removing a stack should work", "[group]")
{
    focus("doc0"_h);
    destroy_doc("doc2"_h);

    REQUIRE(group_.size() == 2);
    REQUIRE_THROWS(group_.stack("doc2"_h));
}

TEST_CASE_METHOD(GroupFixture, "Removing the active stack should reset the active stack index", "[group]")
{
    focus("doc0"_h);
    REQUIRE(group_.active_stack_name() == "doc0"_h);
    destroy_doc("doc0"_h);

    REQUIRE(group_.active_stack_name() == 0);
}