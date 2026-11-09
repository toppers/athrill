# JIT experiment (branch experiment/jit)

This is a throw-away experiment. It is not built unless `ATHRILL_ENABLE_JIT=ON`.

## What the core provides

The core part is `src/main/jit/athrill_jit.h`. It is target-independent and uses
[sljit](https://github.com/zherczeg/sljit) (BSD-2-Clause). It has two parts:

- **Block builder.** A target describes one block of guest instructions with these operations:
  - `mov`, `movs` and `alu` (add, sub, mul, and, or, xor, shifts, div and rem) on CPU-structure fields, context fields, host variables and immediates
  - `jump_if` with labels
  - `call` of a C function, with its int result kept
  - `count`, which counts the executed instructions
  - `exit` and `exit_if`

  The builder turns the block into one host function. The target can emit native operations or call its existing execute function (call-threaded), and it can mix the two within one block.
- **Block cache.** It holds compiled blocks by (pc, key). A block is dropped when the target's generation for that code changes. When the cache is full, it is flushed.

## What the target keeps

The target keeps everything that defines the guest:

- semantics: flags, shift ranges, division by zero, traps
- where a block ends
- clock and counter rules
- precise state: pc and clock are up to date before a call and at an exit
- the dispatch loop, with its budget

The core only guarantees that the host never traps:

- Division must be guarded by the target. The builder refuses an immediate divisor of 0, or of −1 for a signed division.
- Shift counts are masked by (width − 1), as in RISC-V. An Arm target must guard register shifts of 32..255 itself.

## Build and test

```bash
cmake -S test/jit -B build-jit-test -DATHRILL_SLJIT_DIR=../sljit
cmake --build build-jit-test
ctest --test-dir build-jit-test --output-on-failure
```

```bash
test/jit/mutation_check.sh build-jit-test   # the tests must catch 9 deliberate faults
```

The unit tests (`test/jit/jit_test.c`) compare every result with an independent C reference:

- alu for all operations and operand kinds, 32-bit and 64-bit
- mov widening
- all conditions
- `exit_if`
- `call` and its result
- the RISC-V division rules written with guards
- a toy target: an interpreter and a JIT with native and call-threaded instructions give identical state, clock and counts under many budgets, including traps in the middle of a block
- the cache
- refusal of invalid descriptions

Results on 2026-11-09 with sljit 23d64b7:

- macOS arm64: 104,739 checks pass.
- Ubuntu 22.04 x86-64 container: the same checks pass. On this host a missing division guard would kill the process.
- Mutation check: 9 of 9 faults caught.
- The STM32 target built against this core with the option OFF: 694 of 694 ctest pass.

## Next (not started)

RISC-V target:

1. Build blocks from the existing `build_block()`.
2. Make the integer arithmetic native and call the rest.
3. Compare JIT ON and OFF on Erbium: the instruction count (minstret) and the flight logs must be identical.
4. Then measure the RTF.
