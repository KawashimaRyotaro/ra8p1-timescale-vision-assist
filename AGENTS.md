# AGENTS.md

## Project

This repository contains the TRON Programming Contest 2026 project:

"Time-Scale-Adaptive Vision System for Walking Assistance"

Target hardware:
- Renesas EK-RA8P1

RTOS:
- μT-Kernel 3.0 BSP2

Embedded build environment:
- e² studio 2026-04.2
- Renesas FSP 6.5.0

Development workflow:
- VS Code + Codex for source editing, analysis, documentation, and host-side tools
- e² studio for FSP configuration, firmware build, flashing, and hardware debugging

The e² studio project under `firmware/ra8p1/` is the authoritative
embedded project.

## Read first

Before making non-trivial changes, read:

- `docs/PROJECT_CONTEXT.md`
- `docs/ARCHITECTURE.md`
- `docs/ROADMAP.md`
- `docs/DECISIONS.md`
- `docs/BUILD_AND_FLASH.md`

## Core objective

The project implements time-scale-aware visual processing.

High-urgency visual events use a low-latency processing path.

Lower-urgency visual events use a more detailed recognition path.

μT-Kernel task priority scheduling must prevent the detailed
recognition workload from violating the latency requirement of
the high-urgency path.

Real-time scheduling behavior is a core technical contribution,
not merely an implementation detail.

## Source ownership

Application-owned code should normally be created or modified under:

- `firmware/ra8p1/Application/`
- `tools/`
- `tests/`
- `models/`
- `docs/`

Treat the following as generated or third-party code unless explicitly
instructed otherwise:

- `firmware/ra8p1/ra_gen/`
- `firmware/ra8p1/ra_cfg/`
- Renesas FSP source
- μT-Kernel / BSP2 source

## FSP configuration

Do not manually rewrite generated FSP source files.

If a peripheral configuration change is necessary:

1. identify the required FSP module or pin setting;
2. explain the required e² studio / Smart Configurator operation;
3. let the user perform the GUI configuration;
4. inspect the regenerated source afterward.

Do not invent pin assignments or peripheral settings.

## Build authority

Do not claim that the embedded firmware builds or runs successfully
unless the result has actually been verified using the e² studio
project and, where applicable, EK-RA8P1 hardware.

Host-side tests do not replace hardware validation.

## Embedded implementation rules

- Prefer static allocation when maximum sizes are known.
- Avoid unnecessary dynamic allocation.
- Avoid blocking operations in high-priority tasks.
- Keep interrupt handlers short.
- Separate hardware-dependent code from algorithms where practical.
- Use explicit interfaces between capture, motion analysis, inference,
  scheduling, and output modules.
- Do not invent tensor dimensions, model interfaces, hardware registers,
  or device configuration.
- Preserve the known-working BSP baseline.

## Generated and vendor code

Do not reorganize the official BSP merely for stylistic reasons.

Do not move application logic into generated files when it can reside
under `Application/`.

Changes to μT-Kernel core or Renesas FSP internals require an explicit
technical reason.

## GVS safety boundary

GVS-related firmware development begins with a logical command interface,
dummy load, or measurement output.

Human stimulation is not a prerequisite for firmware development.

Do not invent human stimulation parameters.

## Git workflow

`main` must remain in a known buildable state.

For implementation work, prefer short-lived feature branches such as:

- `feat/p0-app-structure`
- `feat/p1-camera`
- `feat/p2-motion`
- `feat/p3-inference`
- `feat/p4-scheduler`
- `feat/p5-output`

Before modifying files:

1. inspect the existing implementation;
2. explain the smallest coherent change;
3. identify files that will be modified.

After modifying files:

1. summarize changed files;
2. identify any required e² studio operations;
3. identify what must be verified on EK-RA8P1;
4. do not claim unperformed hardware validation.

## Development modes

This project has two valid development modes.

### Integration mode

Used when implementing the contest prototype.

In integration mode:
- preserve known-working behavior;
- make small coherent changes;
- keep `main` buildable;
- avoid unrelated scope expansion;
- verify changes on EK-RA8P1 when required.

### Exploration mode

Used to learn RA8P1, edge AI, μT-Kernel, FSP, peripherals,
memory architecture, NPU operation, and related technologies.

In exploration mode:
- experiments may intentionally precede the current roadmap;
- temporary or incomplete implementations are acceptable;
- architecture assumptions may be challenged;
- alternative implementations should be compared when useful;
- experiments do not need to be merged into `main`.

Prefer `exp/*` branches or clearly isolated experimental code.

The purpose of an experiment is not merely to produce working code.
It should answer a technical question or teach something about the platform.

After an experiment, summarize:
1. what was tested;
2. what worked;
3. what failed;
4. what was learned;
5. whether the result should affect the system architecture or roadmap.