#include "AppState.h"

namespace inversa { namespace display {
static AppState s_state;
AppState& app_state() { return s_state; }
}}
