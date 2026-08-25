# teatime-os

Bare-metal ESP8266 OS: own kernel, WiFi driver and network stack, no vendor
SDK linked in. See README.md for what it does and how to build it.

## Build

`cd compile && make` produces `prog.bin` and `prog.irom.bin`. `make test` runs
the host-side crypto test.

## Comments

A comment must explain something the code cannot. If a competent reader could
get it from the code, leave it out.

- Never name a register, bit, mask or literal that the adjacent line already
  contains. Say why the line matters, or say nothing.
- No step labels, no function headers that restate the name, no repeated
  constants. Function headers only where they state a contract for callers.
- Describe what the hardware needs, not where a value came from.
- Worth writing: ordering that must not change, a read whose bus transaction
  matters, a deliberately absent step, a short header for a genuinely
  non-trivial mechanism.
- Voice: `//`, lowercase, one sentence, `<fact>, so <consequence>`. A single
  space before a trailing comment.

## Memory barriers

The compiler emits `memw` before every volatile access, so consecutive
`WRITE_REG` calls are already ordered. An explicit `asm volatile("memw")` is
only needed to order ordinary memory writes before an MMIO write.

## Conventions

**Concurrency**
- Only kernel code calls `disable()`/`enable()`. Shared state is guarded by a
  mutex unless the other side is an interrupt.
- NMI handlers must not touch shared kernel structures. They are truly
  non-maskable, so they can interrupt a kernel task that has masked interrupts
  to update those structures and see them half-written.
- Each layer of the stack owns one concern. For example: the scheduler
  only schedules based on what processes are available, it does not also move
  processes into a ready state.
- Timing uses the sleep list: `sleep(ticks)` and `cond_timedwait`. Never poll
  a clock.
- No kernel service should have an unbounded wait.

**Style**
- Struct-cast wire headers over 4-aligned buffers; lengths from `sizeof`;
  every protocol literal named; one constant, one meaning.
- `switch` cases indented one level. One declaration per line. No forward
  declarations of statics. Word-sized values assigned, never memcpy'd.
- Established names only; no digits in names, no invented shorthand.
