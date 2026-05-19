## Learned User Preferences

- User writes queries in Portuguese; respond in Portuguese when the user's message is in Portuguese.
- When generating code, apply changes directly to files without explaining what was changed.
- User expects direct code fixes with no preamble or rationale comments.

## Learned Workspace Facts

- PlatformIO-based embedded firmware project (C++), targeting ESP32 or similar MCU.
- Uses an event-bus architecture: typed `Event` / `EventType` structs, e.g. `RampedSetpointChanged`, `HeaterStateChanged`, `BLESend`.
- BLE payloads must use constructor form `Event(EventType::BLESend, json)` — brace-initialization bypasses the `String` constructor overload.
- `stopRamp()` must check `_ramping` before clearing it so the final setpoint is published via `RampedSetpointChanged`.
- Scheduler cancel must snapshot `_heatingStarted` before clearing flags to correctly publish `HeaterStateChanged(false)`.
- Ziegler-Nichols auto-tune accumulates period and amplitude once per full cycle (`_peakCount % 2 == 0`), not on every peak.
- GitHub Actions CI workflow (`Firmware CI`) triggers on `v*` tags only.
- Android SDK is installed at `~/Library/Android/sdk`; PATH entries for `platform-tools`, `emulator`, `tools`, and `tools/bin` were added to `~/.zshrc`.
