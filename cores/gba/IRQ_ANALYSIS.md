# GBA IRQ Handling Analysis and Bug Report

## Executive Summary

Pokemon Fire Red fails to boot because **LR_irq becomes 0** when returning from IRQ, causing PC to become 0xFFFFFFFC (which is -4) after `SUBS PC, LR, #4` executes.

## Reference Emulator Comparison

### 1. mGBA IRQ Entry (`ARMRaiseIRQ`)

```c
// Save CPSR first
uint32_t cpsr = cpu->cpsr;
// Switch mode (banks registers)
ARMSetPrivilegeMode(cpu, MODE_IRQ);
// Save old CPSR to SPSR
cpu->spsr = cpsr;
// Set LR: PC - instructionWidth + 4
cpu->gprs[ARM_LR] = cpu->gprs[ARM_PC] - instructionWidth + WORD_SIZE_ARM;
// Disable IRQ, switch to ARM mode
cpu->cpsr.i = 1;
cpu->cpsr.t = 0;
// Jump to vector
cpu->gprs[ARM_PC] = BASE_IRQ;
```

**Key observations:**
1. **SPSR is saved AFTER mode switch** - the mode switch banks the old LR, then SPSR gets the old CPSR
2. **LR calculation**: `LR = PC - instructionWidth + 4`
   - If ARM mode (instructionWidth=4): `LR = PC`
   - If Thumb mode (instructionWidth=2): `LR = PC + 2`

### 2. SkyEmu IRQ Entry (`arm7_process_interrupts`)

```c
// LR_irq directly receives PC+4
cpu->registers[R14_irq] = cpu->registers[PC] + 4;
// SPSR_irq directly receives old CPSR
cpu->registers[SPSR_irq] = cpsr;
// Update CPSR mode bits
cpu->registers[CPSR] = (cpsr & 0xffffffE0) | 0x12;
```

**Key observation:** SkyEmu uses **direct register file access** - it writes to `R14_irq` directly, NOT to `R14` and then banking. This avoids any ordering issues.

### 3. NanoBoyAdvance IRQ Entry (`SignalIRQ`)

```c
// Save CPSR to SPSR_IRQ first
state.spsr[BANK_IRQ].v = state.cpsr.v;
// Switch mode (which banks registers)
SwitchMode(MODE_IRQ);
// Set LR based on instruction set
if (thumb) SetReg(14, state.r15);
else SetReg(14, state.r15 - 4);
```

**Key observation:** Saves SPSR **before** mode switch, then sets LR **after** mode switch.

## Our Current Implementation Issues

### Issue 1: Register Banking Order Problem

In `enter_exception()`:

```cpp
// Switch mode - this BANKS the old R14 and RESTORES m_irq_regs[1] (which is 0!)
switch_mode(mode);

// Save old CPSR to SPSR
set_spsr(old_cpsr);

// Set return address in LR - but we're NOW IN IRQ MODE
// This writes to m_regs[14] which is LR_irq, not LR_sys!
m_regs[14] = m_regs[15] + 4;  // This is correct for LR_irq
```

The sequence is:
1. `switch_mode(IRQ)` calls `bank_registers(System, IRQ)`
2. `bank_registers` saves System's R14 to `m_usr_sp_lr[1]` (good)
3. `bank_registers` restores IRQ's R14 from `m_irq_regs[1]` (which is 0!)
4. We then set `m_regs[14] = PC + 4` - this correctly sets LR_irq

Wait... this should work! Let me check more carefully...

### The REAL Issue: LR_irq is Being Overwritten

Looking at the debug output:
```
[GBA] set_cpsr MODE CHANGE: System -> IRQ (value=0x00000092, PC=0x030036C0)
[GBA] set_cpsr MODE CHANGE: IRQ -> System (value=0x6000003F, PC=0xFFFFFFFC)
```

The PC becomes 0xFFFFFFFC when returning to System mode. This means when we execute `SUBS PC, LR, #4` in IRQ mode, LR_irq is 0x00000000.

**Hypothesis**: Something is overwriting LR_irq after we set it.

### Possible Causes:

1. **HLE BIOS handler corrupts LR_irq**: Our BIOS handler at 0x18 does `STMFD SP!, {R0-R3, R12, LR}` which should save LR_irq to the IRQ stack, but maybe it's not working correctly?

2. **LR_irq initialized to 0**: In `reset()`, we set `m_irq_regs.fill(0)` but never initialize LR_irq (index 1). When the first IRQ fires, we set LR correctly, but...

3. **Nested IRQ or mode switch problem**: If the game's IRQ handler switches modes (e.g., to call SWI), there might be register banking issues.

### Issue 2: LR Calculation May Be Wrong

Reference emulators vary:

| Emulator | ARM Mode LR | Thumb Mode LR |
|----------|-------------|---------------|
| mGBA     | PC          | PC + 2        |
| SkyEmu   | PC + 4      | PC + 4        |
| NanoBoyAdvance | PC - 4 | PC          |

Our current code:
- ARM: `LR = PC + 4` (matches SkyEmu)
- Thumb: `LR = PC + 2` (matches mGBA)

But wait - our PC semantics might differ from reference emulators. In ARM7TDMI:
- PC during execution points to current instruction + 8 (ARM) or + 4 (Thumb)
- After instruction execution, PC points to next instruction

If our PC points to "next instruction to execute" when IRQ fires:
- mGBA formula: `LR = PC - instr_width + 4`
  - ARM: `LR = (next_instr) - 4 + 4 = next_instr`
  - Thumb: `LR = (next_instr) - 2 + 4 = next_instr + 2`

So to return to `next_instr`:
- `SUBS PC, LR, #4` in ARM mode: `PC = LR - 4 = next_instr - 4` (WRONG!)
- We need `LR = next_instr + 4` so `SUBS PC, LR, #4` gives `PC = next_instr`

**Our LR calculation is correct for the semantics.**

### Issue 3: The Debug Output Shows the Problem

```
PC=0x030036C0 when entering IRQ
PC=0xFFFFFFFC when returning from IRQ
```

0xFFFFFFFC = 0 - 4. So LR was 0 when `SUBS PC, LR, #4` executed.

The question is: **where did LR_irq become 0?**

## Root Cause Analysis

Looking at the BIOS HLE code in `bus.cpp`:

```cpp
// IRQ handler at 0x18
write_word(0x18, 0xE92D500F);  // stmfd sp!, {r0-r3, r12, lr}
write_word(0x1C, 0xE3A00301);  // mov r0, #0x04000000
write_word(0x20, 0xE28FE000);  // add lr, pc, #0  (LR = PC+8 = 0x28)
write_word(0x24, 0xE510F004);  // ldr pc, [r0, #-4]  (jump to [0x03FFFFFC])
write_word(0x28, 0xE8BD500F);  // ldmfd sp!, {r0-r3, r12, lr}
write_word(0x2C, 0xE25EF004);  // subs pc, lr, #4  (return from IRQ)
```

The flow is:
1. IRQ fires, enter_exception sets LR_irq = return_addr
2. CPU jumps to 0x18 (BIOS IRQ handler)
3. 0x18: `STMFD SP!, {R0-R3, R12, LR}` - saves LR_irq to stack
4. 0x20: `ADD LR, PC, #0` - LR = 0x28 (return address for game handler)
5. 0x24: `LDR PC, [R0, #-4]` - jump to game's IRQ handler
6. (game handler runs)
7. 0x28: `LDMFD SP!, {R0-R3, R12, LR}` - restores LR_irq from stack
8. 0x2C: `SUBS PC, LR, #4` - returns from IRQ

**CRITICAL ISSUE FOUND**: The game's IRQ handler might be overwriting LR_irq!

If the game's IRQ handler doesn't preserve LR (common for handlers that return with `BX LR`), it will corrupt LR_irq. When we return to BIOS at 0x28, we restore the saved LR, but if the stack got corrupted or SP_irq is wrong, LR becomes garbage.

## The Likely Bug

**SP_irq is probably not set correctly or the stack operations are failing.**

Looking at reset():
```cpp
m_irq_regs[0] = 0x03007FA0;  // SP_irq
```

But in bank_registers, when we enter IRQ mode:
```cpp
case ProcessorMode::IRQ:
    m_regs[13] = m_irq_regs[0];  // This should be 0x03007FA0
    m_regs[14] = m_irq_regs[1];  // This is 0 initially
```

Wait - the issue might be that we never actually bank System mode's registers before entering IRQ for the first time!

## Detailed Fix Required

1. **Verify SP_irq is correct when entering IRQ mode**
2. **Verify the STMFD/LDMFD at BIOS handler work correctly**
3. **Consider following SkyEmu's approach**: Directly write to banked registers instead of relying on banking.

## Recommended Fixes

### Fix 1: Set LR_irq before banking (like SkyEmu)

```cpp
void ARM7TDMI::enter_exception(ProcessorMode mode, uint32_t vector) {
    uint32_t old_cpsr = m_cpsr;

    // Calculate return address BEFORE mode switch
    uint32_t return_addr;
    if (old_cpsr & FLAG_T) {
        return_addr = m_regs[15];  // Thumb: next instruction
    } else {
        return_addr = m_regs[15];  // ARM: next instruction
    }

    // For IRQ, add 4 so SUBS PC, LR, #4 returns correctly
    if (mode == ProcessorMode::IRQ) {
        return_addr += 4;
    }

    // Save to the TARGET mode's banked LR directly
    if (mode == ProcessorMode::IRQ) {
        m_irq_regs[1] = return_addr;
    } else if (mode == ProcessorMode::Supervisor) {
        m_svc_regs[1] = return_addr;
    }
    // ... etc

    // Now switch mode (which will restore this value into R14)
    switch_mode(mode);

    // Save SPSR
    set_spsr(old_cpsr);

    // Now m_regs[14] should have the correct return address
    // ...
}
```

### Fix 2: Alternatively, set LR after switch but BEFORE reading banked values

In `bank_registers`, we should NOT restore R14 for the new mode because we're about to set it.

Actually, looking at the code flow again:
1. `switch_mode(mode)` calls `bank_registers(old_mode, new_mode)`
2. `bank_registers` saves old mode's R14, then restores new mode's R14 from bank
3. Back in `enter_exception`, we set `m_regs[14] = return_addr`

The issue is step 2 - we restore R14 from bank (which is 0 for first IRQ), then step 3 overwrites it with the correct value. So that's fine...

Unless the game's IRQ handler is the problem!

## Investigation: Check the game's IRQ handler

The game's handler at 0x030036C0 (from the debug output) might be returning incorrectly. Need to trace what happens between entering and exiting IRQ mode.

## CRITICAL BUG FOUND

After deeper analysis, I found the issue. In `enter_exception()`:

```cpp
// Switch mode - this changes m_regs[14] to LR_irq (restores from bank)
switch_mode(mode);

// Save old CPSR to SPSR (correct)
set_spsr(old_cpsr);

// Set LR - BUT THIS USES m_regs[15] WHICH IS STILL THE OLD PC
// m_regs[15] hasn't been changed yet!
if (old_cpsr & FLAG_T) {
    m_regs[14] = m_regs[15] + 2;  // m_regs[15] = old_pc, correct
} else {
    m_regs[14] = m_regs[15] + 4;  // m_regs[15] = old_pc, correct
}
```

Wait, that looks correct. Let me trace through more carefully:

1. IRQ fires at PC=0x030036C0
2. `enter_exception(IRQ, 0x18)` is called
3. `old_cpsr = m_cpsr` (System mode, 0x0000001F)
4. `old_pc = m_regs[15]` = 0x030036C0
5. `switch_mode(IRQ)` is called
   - `bank_registers(System, IRQ)` banks System's R13/R14
   - Restores R13/R14 from IRQ bank (m_irq_regs[0]/[1])
   - m_irq_regs[1] is 0 on first IRQ!
   - Now `m_regs[14] = 0`
6. `set_spsr(old_cpsr)` - saves System CPSR to SPSR_irq
7. `m_regs[14] = m_regs[15] + 4` = 0x030036C0 + 4 = 0x030036C4
   - This OVERWRITES the 0 with the correct value!
8. PC is set to 0x18, pipeline is flushed

So the sequence IS correct for first IRQ. The problem must be somewhere else...

Oh wait! I just realized - look at step 5 more carefully:

```cpp
void ARM7TDMI::bank_registers(ProcessorMode old_mode, ProcessorMode new_mode) {
    // ...
    // Save current R13-R14 to old mode's bank
    switch (old_mode) {
        case ProcessorMode::User:
        case ProcessorMode::System:
            m_usr_sp_lr[0] = m_regs[13];  // System's SP
            m_usr_sp_lr[1] = m_regs[14];  // System's LR
            break;
        // ...
    }

    // Restore R13-R14 from new mode's bank
    switch (new_mode) {
        case ProcessorMode::IRQ:
            m_regs[13] = m_irq_regs[0];  // IRQ's SP = 0x03007FA0 (correct)
            m_regs[14] = m_irq_regs[1];  // IRQ's LR = 0 initially (overwritten later)
            break;
        // ...
    }
}
```

After bank_registers, m_regs[14] = 0. Then in enter_exception, we set m_regs[14] = PC + 4.
This is correct!

BUT WAIT - what if it's the SECOND or THIRD IRQ that's failing?

On the first IRQ:
1. LR_irq gets set to return_address (correct)
2. BIOS handler saves it to stack
3. Calls game handler
4. Returns, restores LR_irq from stack
5. SUBS PC, LR, #4 returns correctly

On the second IRQ:
1. `bank_registers(System, IRQ)` is called
2. It saves System's LR to m_usr_sp_lr[1]
3. It restores IRQ's LR from m_irq_regs[1]
4. **BUT m_irq_regs[1] STILL HAS THE OLD VALUE FROM FIRST IRQ!**
   - After first IRQ completed, we did set_cpsr(get_spsr()) which calls switch_mode
   - switch_mode calls bank_registers(IRQ, System)
   - This SAVES m_regs[14] (which is LR_irq from LDMFD) to m_irq_regs[1]!
   - So m_irq_regs[1] should have the correct return address value

Hmm, let me trace through what happens when SUBS PC, LR, #4 executes:

1. rd = 15, S = 1
2. result = m_regs[14] - 4 = LR_irq - 4
3. m_regs[15] = result
4. `set_cpsr(get_spsr())` is called
   - This calls switch_mode(System) internally
   - switch_mode calls bank_registers(IRQ, System)
   - bank_registers SAVES m_regs[14] to m_irq_regs[1]
   - **BUT m_regs[14] is still LR_irq at this point!**
   - Then it RESTORES m_usr_sp_lr[1] into m_regs[14]

So after SUBS PC, LR, #4:
- m_regs[15] = LR_irq - 4 (return address - correct)
- m_irq_regs[1] = LR_irq (the value it was BEFORE LDMFD restored it)

**WAIT!** Let me check the LDMFD instruction. The BIOS does:

```
0x28: LDMFD SP!, {R0-R3, R12, LR}
```

This loads LR from the stack. So after this instruction:
- m_regs[14] = value loaded from stack (the original return address)

Then:
```
0x2C: SUBS PC, LR, #4
```

This:
1. Computes result = m_regs[14] - 4
2. Stores result in PC
3. Calls set_cpsr(get_spsr())
   - switch_mode(System)
   - bank_registers(IRQ, System) saves m_regs[14] to m_irq_regs[1]

So m_irq_regs[1] ends up with the CORRECT return address (the one LDMFD restored).

For the NEXT IRQ:
1. enter_exception is called
2. switch_mode(IRQ) banks System's regs, restores IRQ's regs
3. m_regs[14] = m_irq_regs[1] = the OLD return address from last IRQ!
4. Then we OVERWRITE m_regs[14] with the NEW return address

So it should be correct...

## NEW THEORY

Maybe the issue is the instruction at 0x2C isn't being recognized as the IRQ return?

Let me check - our BIOS HLE writes:
```cpp
write_word(0x2C, 0xE25EF004);  // subs pc, lr, #4
```

0xE25EF004:
- Condition: 0xE = AL (always)
- 0x25E = 0010 0101 1110
  - bits 27-26 = 00 (data processing)
  - bit 25 = 1 (immediate operand)
  - bits 24-21 = 0010 = SUB
  - bit 20 = 1 = S (set flags)
  - Rn = 0xE = R14 = LR
  - Rd = 0xF = R15 = PC
- Operand2 = 0x004 = immediate 4

So it's `SUBS PC, LR, #4` which is correct.

But wait - where is the game's IRQ handler returning TO? If it returns to 0x28 (via BX LR where LR was set to 0x28), it should work...

Unless the game's handler corrupts the stack or SP_irq!

## FOUND THE REAL BUG!

Looking at the BIOS handler again:
```
0x20: add lr, pc, #0  (LR = PC+8 = 0x28)
0x24: ldr pc, [r0, #-4]  (jump to [0x03FFFFFC], which mirrors 0x03007FFC)
```

The game's handler is at address stored at 0x03007FFC (aka 0x03FFFFFC).
LR is set to 0x28, so when game handler does BX LR, it returns to 0x28.

At 0x28:
```
0x28: ldmfd sp!, {r0-r3, r12, lr}
```

This pops 6 registers from SP_irq. But what if SP_irq is corrupted or the stack wasn't set up correctly?

Let me check our reset() again:
```cpp
m_irq_regs[0] = 0x03007FA0;  // SP_irq
```

So SP_irq = 0x03007FA0. The BIOS handler does:
- STMFD SP!, {R0-R3, R12, LR} which decrements SP by 24 and stores 6 registers
- SP_irq becomes 0x03007FA0 - 24 = 0x03007F88

When LDMFD happens, it reads from 0x03007F88 and increments SP back to 0x03007FA0.

**THE BUG**: What if the memory at 0x03007F88 isn't being written correctly, OR is being corrupted by the game?

Actually, let me look at what happens if there's NO game handler installed at 0x03007FFC...

## CRITICAL INSIGHT

The address 0x03007FFC is in IWRAM. On reset, IWRAM is zeroed. So [0x03007FFC] = 0.

When BIOS handler does `LDR PC, [R0, #-4]` where R0 = 0x04000000:
- Read from 0x04000000 - 4 = 0x03FFFFFC
- 0x03FFFFFC mirrors to 0x03007FFC in IWRAM
- This reads... 0 (if game hasn't set the handler yet!)

So PC becomes 0, and the CPU starts executing from address 0 (BIOS).

But wait - the game SHOULD set up its IRQ handler before enabling interrupts. So this shouldn't be the issue for Pokemon Fire Red...

Unless the issue is timing - maybe an IRQ fires before the game has set up its handler?

## SUMMARY OF POTENTIAL ISSUES

1. **IRQ fires too early**: Before game sets up handler at 0x03007FFC
2. **Stack corruption**: SP_irq or the stack memory is wrong
3. **Mode switch during game handler**: Game calls SWI which corrupts IRQ state
4. **LR calculation wrong**: Though I've verified it matches references...
5. **BIOS HLE not executing properly**: Something wrong with instruction execution at 0x18-0x2C

## RECOMMENDED FIX

Use SkyEmu's approach: write to banked registers directly:

```cpp
void ARM7TDMI::enter_exception(ProcessorMode mode, uint32_t vector) {
    uint32_t old_cpsr = m_cpsr;

    // Calculate LR for the target mode FIRST
    uint32_t lr_value;
    if (mode == ProcessorMode::IRQ || mode == ProcessorMode::FIQ) {
        // IRQ/FIQ: LR = PC + 4 so SUBS PC, LR, #4 returns to next instruction
        lr_value = m_regs[15] + 4;
    } else {
        // SWI/UND/ABT: LR = PC so MOVS PC, LR returns to next instruction
        lr_value = m_regs[15];
    }

    // Write directly to the banked LR for target mode
    switch (mode) {
        case ProcessorMode::IRQ:
            m_irq_regs[1] = lr_value;
            break;
        case ProcessorMode::FIQ:
            m_fiq_regs[6] = lr_value;
            break;
        case ProcessorMode::Supervisor:
            m_svc_regs[1] = lr_value;
            break;
        // etc
    }

    // Also write directly to banked SPSR
    switch (mode) {
        case ProcessorMode::IRQ:
            m_spsr_irq = old_cpsr;
            break;
        // etc
    }

    // NOW switch mode (this will restore the values we just wrote)
    switch_mode(mode);

    // Disable IRQ, clear Thumb
    m_cpsr |= FLAG_I;
    m_cpsr &= ~FLAG_T;

    // Jump to vector
    m_regs[15] = vector;
    flush_pipeline();
}
```

This ensures the banked registers have correct values BEFORE the mode switch restores them.

## ADDITIONAL INVESTIGATION (2026-01-03)

After further analysis, I verified that:

1. **BIOS HLE instructions are correctly encoded**:
   - 0x18: STMFD SP!, {R0-R3, R12, LR} (0xE92D500F) - correct
   - 0x1C: MOV R0, #0x04000000 (0xE3A00301) - correct
   - 0x20: ADD LR, PC, #0 (0xE28FE000) - sets LR = PC + 8 = 0x28, correct
   - 0x24: LDR PC, [R0, #-4] (0xE510F004) - loads from 0x03FFFFFC, correct
   - 0x28: LDMFD SP!, {R0-R3, R12, LR} (0xE8BD500F) - correct
   - 0x2C: SUBS PC, LR, #4 (0xE25EF004) - correct

2. **LR calculation issue**: The original code had a potential Thumb-mode bug:
   - Old: Thumb LR = PC + 2, ARM LR = PC + 4
   - Fixed: Both use LR = PC + 4 (since SUBS PC, LR, #4 is always executed in ARM mode)

3. **The core issue may be elsewhere**:
   - Game's IRQ handler might be corrupting the stack
   - IRQ handler at 0x03007FFC might not be set when first IRQ fires
   - Timing issue where IRQ fires before game sets up handler

## FIX IMPLEMENTED

Changed `enter_exception()` to use `LR = PC + 4` for both ARM and Thumb modes for IRQ/FIQ exceptions, matching the ARM7TDMI specification where LR_irq = "address of next instruction" + 4.

## FIXES VERIFIED

All GBA test suite tests pass after the changes:
- ARM instruction tests: PASSED
- Thumb instruction tests: PASSED
- Memory tests: PASSED
- BIOS tests (including IRQ): PASSED

The key fix was changing the LR calculation in `enter_exception()`:
- **Before**: Thumb mode used `LR = PC + 2`, ARM mode used `LR = PC + 4`
- **After**: Both modes use `LR = PC + 4` for IRQ/FIQ exceptions

This matches the ARM7TDMI specification where LR_irq = "address of next instruction" + 4, regardless of instruction set. The SUBS PC, LR, #4 return instruction is always executed in ARM mode.

## REMAINING INVESTIGATION

For Pokemon Fire Red specifically, if issues persist after these fixes:
1. Check timing of when IRQs fire relative to game initialization
2. Verify the game's IRQ handler at 0x03007FFC is set before first IRQ
3. Check for nested interrupt or mode switch issues
4. Consider game-specific timing requirements for VBlank
