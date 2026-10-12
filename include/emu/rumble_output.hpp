#pragma once

namespace emu {
// Optional core output sampled by the frontend after an emulated frame.
// Physical feedback is a presentation effect; the core owns deterministic
// motor state so snapshots and netplay rollback can restore it.
class IRumbleOutput {
public:
    virtual ~IRumbleOutput() = default;
    virtual bool rumble_active(unsigned port) const = 0;
};
}
