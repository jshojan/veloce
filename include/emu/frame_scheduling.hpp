#pragma once
#include <cstdint>

namespace emu {
enum class FrameScheduling : uint8_t { InstructionQuantum, VideoField };

// Optional capability, separate from the core ABI. N64 diagnostics retain the
// historical 100,000-attempt budget; new native sessions use VI field endings.
// Changing this explicitly migrates the current execution schedule. The mode
// is part of the native save state, including TAS and rollback checkpoints.
class IFrameScheduling {
public:
    virtual ~IFrameScheduling() = default;
    virtual FrameScheduling get_frame_scheduling() const = 0;
    virtual void set_frame_scheduling(FrameScheduling mode) = 0;
};
}
