#ifndef STATEMACHINE_H
#define STATEMACHINE_H
#include <Arduino.h>
class StateMachine;

class State {
public:
    virtual ~State() = default;
    State(const char *name): name(name) {}
    State(): name(nullptr) {}

    virtual void enter() {}
    virtual void exit(){}
    virtual void run(){}

    const char *name = nullptr;
    StateMachine *machine = nullptr;
};

class StateMachine {
public:
    StateMachine(void (*on_change)())
        : on_change(on_change) {
            previous_state = &idle;
            state = &idle;
        }

        StateMachine(State *state) : state(state){}

        StateMachine(State *state, void (*on_change)())
            : on_change(on_change), state(state){}

        void init(State *state)
        {
            this->state = state;
            this->previous_state = state;
            state->enter();
            state->run();
    }

    void setState(State *state) {
        Serial.print("Setting state: ");
        this->state = state;
        Serial.println(state->name);
    }

    bool run() {
        if (previous_state == nullptr) {
            previous_state = state;
                state->enter();
                state->run();
            return true;
        }
        
        if (state != previous_state) {
            previous_state->exit();
            previous_state = state;
            state->enter();
            state->run();
            return true;
        }

        state->run();
        return false;
    }

    void setIdleState() {
        Serial.println("Setting idle state");
        this->state = &idle;
    }

    State* getCurrentState() const { return state; }

private:
    State idle;
    State *previous_state;
    State *state;
    void (*on_change)();
};

#endif // STATEMACHINE_H
