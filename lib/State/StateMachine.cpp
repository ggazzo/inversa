#include "StateMachine.h"


// struct State
// {
//   State(void (*on_enter)(State *state), void (*on_state)(State *state), void (*on_exit)(State *state), void (*on_check)(State *state), const char *name = nullptr);
//   State(void (*on_enter)(State *state));
//   State(void (*on_enter)(State *state), const char *name);
// #ifdef DEBUG
//   const char *name;
// #endif
//   void (*on_enter)(State *state);
//   void (*on_state)(State *state);
//   void (*on_exit)(State *state);
//   void (*on_check)(State *state);
// };

// State::State(void (*on_enter)(State *state), void (*on_state)(State *state), void (*on_exit)(State *state), void (*on_check)(State *state), const char *name)
// : on_enter(on_enter),
//   on_state(on_state),
//   on_exit(on_exit),
//   #ifdef DEBUG
//   name(name),
//   #endif
//   on_check(on_check)
// {
// }

// State::State(void (*on_enter)(State *state), const char *name): State(on_enter, nullptr, nullptr, nullptr, name) {}

// State::State(void (*on_enter)(State *state)): State(on_enter, nullptr, nullptr, nullptr) {}
// template <typename T> struct StateDynamic: State {
//   StateDynamic(T *state, void (*on_enter)(
//     StateDynamic<T> * state
//   // ), void (*on_state)(StateDynamic<T> * state), void (*on_exit)(), void (*on_check)()) : State(on_enter, on_state, on_exit, on_check), state(state) {}
//   ), void (*on_state)(StateDynamic<T> * state), void (*on_exit)(StateDynamic<T> * state), void (*on_check)(StateDynamic<T> * state)) : State((void (*)(State *state))on_enter, (void (*)(State *state))on_state, (void (*)(State *state))on_exit, (void (*)(State *state))on_check), state(state) {}
//   StateDynamic(char *name, T *state, void (*on_enter)(
//     StateDynamic<T> * state
//   // ), void (*on_state)(StateDynamic<T> * state), void (*on_exit)(), void (*on_check)()) : State(on_enter, on_state, on_exit, on_check), state(state) {}
//   ), void (*on_state)(StateDynamic<T> * state), void (*on_exit)(StateDynamic<T> * state), void (*on_check)(StateDynamic<T> * state)) : State((void (*)(State *state))on_enter, (void (*)(State *state))on_state, (void (*)(State *state))on_exit, (void (*)(State *state))on_check, name), state(state) {}
//   T *state;
// };


// struct StateMachine {
//     StateMachine(State *state, void (*on_change)()) : state(state), on_change(on_change) {}
//     State *state;
//     State *previous_state;
//     void (*on_change)();


//   void init(State *state) {
//     this->state = state;
//     this->previous_state = state;
//     if (state->on_enter != nullptr)
//     state->on_enter(state);
//     if (state->on_check != nullptr)
//     state->on_check(state);
//   }

//   void on_state() {
//     if (state->on_state != nullptr)
//       state->on_state(state);
//   }

//   bool run(){
//       if (previous_state == nullptr)
//         {
//             previous_state = state;
//             if (state->on_enter != nullptr)
//             state->on_enter(state);
//             if (state->on_check != nullptr)
//             state->on_check(state);
//             return true;
//         }

//         if (state != previous_state)
//         {
//             if(previous_state->on_exit != nullptr){
//               previous_state->on_exit(previous_state);
//             }
            
//             previous_state = state;
//             if (state->on_enter != nullptr){
//               state->on_enter(state);
//             }
//             if (state->on_check != nullptr){
//               state->on_check(state);
//             }
//             return true;
//         }

//         if (state->on_check != nullptr)
//         state->on_check(state);
//         return false;
//   }
// };