# Repository Guidelines

This guide helps contributors work productively on this mini x86-32 educational OS.

## Project Structure & Module Organization
- `include/` C headers (`kernel.h`, `keyboard.h`, `debug_utils.h`)
- `src/` implementation
  - `kernel.c` (interrupts, scheduler, display, serial)
  - `keyboard.c` (PS/2 input, SPSC ring buffer, `read_line`)
  - `debug_utils.c` (logging, metrics, profiling)
  - `boot/` (`boot.s`, `kernel_entry.s`, `interrupt.s`)
- `tests/` QEMU-driven function tests and harness
- `linker/` `kernel.ld`
- `docs/` Architecture, policy, and quality reports

## Build, Test, and Development Commands
- `make all` → builds `os.img`
- `make run` / `make run-nogui` / `make run-noserial` → run in QEMU
- `make analyze` → cppcheck + clang static analyzer
- `make quality` → extra checks (magic numbers, long functions)
- `make test` → run all test images (headless via QEMU)
- `make test-pic|test-thread|test-interrupt|test-sleep` → focused tests
- `make clean` / `make test-clean` → remove build/test artifacts
- `make check-env` → verify toolchain (i686-elf-gcc, nasm, qemu)

## Coding Style & Naming Conventions
- Language: C (gnu99), NASM (elf32/bin)
- Indentation: 4 spaces, no tabs
- Naming: `snake_case` for functions/vars; `UPPER_SNAKE_CASE` for macros/consts
- Headers expose API; implementations remain in `src/`
- Keep constants in headers (PIC/PIT/IDT/VGA) — avoid magic numbers

## Testing Guidelines
- Tests live in `tests/`; each area has a pair: driver + kernel-side test
- Run locally: `make test` or a specific `make test-...`
- Add tests for scheduler/interrupt/keyboard changes; prefer headless QEMU
- Keep tests deterministic; use serial logs for assertions when possible

## Commit & Pull Request Guidelines
- Commits: imperative subject (≤ 72 chars), body for rationale/impact
- Reference issues (e.g., `Fixes #123`) and affected modules
- PRs must include:
  - What changed and why; run output (short log) or screenshots
  - Test plan (commands) and results; added/updated tests
  - Docs updated when behavior or APIs change (see below)

## Documentation Requirements
- Update: `docs/ARCHITECTURE_ja.md` (diagrams intact), `docs/SCHEDULER_POLICY_ja.md`, and quality docs when modifying behavior (e.g., scheduler policy, input API, memory layout)
- Keep A20 and existing charts intact; add deltas only.
