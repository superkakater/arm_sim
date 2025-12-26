# arm_sim - ARM Simulator

This is a small, debugger-like ARM **subset** simulator that:
- fetches 32-bit instruction words from memory
- decodes & executes them
- prints **PC / instruction / registers / memory window** in a terminal UI
- provides a command shell (`help`) to inspect and patch state

It implements the isntruction formats and the base opcodes from the *Computer Organization and Design: the hardware/software interface: ARM edition*

On top of that, it adds a **small high-impact set** of extra instructions:
- **CMP** (sets flags: Z/N) + **B.EQ / B.NE / B.LT / B.GE**
- **EOR**
- **LSL / LSR**
- **MUL**
- **BL / RET**
- **NOP / HALT**

> Note: extra instructions use a documented *custom encoding* that doesn’t conflict with the course sheet opcodes.

---

## Build

```bash
make
```

Run:

```bash
./arm
```

Clean:

```bash
make clean
```

---

## Program files (`.arm`)

A `.arm` file is plain text containing **one 32-bit hex word per line**, e.g.:

```
0xD10003E0
0x91000401
0x8B010000
```

`load fname[.arm]` loads words into memory starting at address 0.

---

## REPL commands

Type `help` in the simulator:

- `memory hex` / `memory dec` / `memory code`
- `PC=#00`                   (set PC in bytes; e.g., `PC=#40`)
- `M[#]=#`                   (write memory word; e.g., `M[#16]=#123`)
- `R[#]=#` or `X#=#`         (write register; e.g., `X3=#99`)
- `save fname[.arm]`
- `load fname[.arm]`
- `title your title here`
- `clear registers` / `clear memory` / `clear`
- `break [#addr]` / `break list` / `break del #addr` / `break toggle #addr` / `break clear`
- `step [n]` (executes n instructions; stops before the next breakpoint)
- `continue` / `cont` / `c` (continue execution; steps once if currently on a breakpoint)
- `run [fast|slow] [nsteps]` (default: `slow` runs 20 steps; `fast` runs until HALT)

---

## Typing instructions directly

You can also type an instruction line (e.g., `ADDI X1, X0, #5`).
The simulator will:
1) assemble it into a 32-bit word at the **current PC**,
2) store it into memory, and
3) **advance PC by 4** so you can type the next instruction.

It **does not execute** the instruction immediately. To execute, set `PC=#0` (or wherever) and use `run`.

Supported mnemonics:
- Base sheet: `ADD, SUB, ADDI, SUBI, LDUR, STUR, B, CBZ, CBNZ`
- Extras: `CMP, B.EQ, B.NE, B.LT, B.GE, AND, ORR, EOR, LSL, LSR, MUL, BL, RET, NOP, HALT`

---

## Notes on memory & registers

- Registers are 64-bit: `X0`..`X31`
- Memory is word-addressed in this project (4 bytes per word), but addresses are written in bytes.
- PC is byte-addressed and normally advances by 4 each step.

---

## Example session

```text
> title demo
> X0=#10
> X1=#20
> PC=#0
> ADD X2, X0, X1   ; stored at M[000]
> HALT            ; stored at M[004]
> PC=#0
> run
> save demo.arm
```
