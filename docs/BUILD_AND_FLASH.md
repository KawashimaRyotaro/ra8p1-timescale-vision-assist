# Build and Flash

## Verified environment

Target:
- Renesas EK-RA8P1

IDE:
- e² studio 2026-04.2

FSP:
- Renesas FSP 6.5.0

RTOS:
- μT-Kernel 3.0 BSP2

Project directory:

`firmware/ra8p1/`

## Build

The authoritative embedded build is performed from e² studio.

General procedure:

1. Open the configured e² studio workspace.
2. Select the EK-RA8P1 μT-Kernel project.
3. Build Project.
4. Confirm that the build completes without errors.

Do not infer build success from source inspection alone.

## Hardware debugging

Debugger:
- SEGGER J-Link

General procedure:

1. Connect EK-RA8P1 to the development PC.
2. Start the Renesas GDB Hardware Debugging configuration.
3. Flash the current firmware.
4. Run or debug the application.
5. Confirm expected execution on hardware.

## Verification rule

A change is not considered hardware-verified until the result has been
observed on EK-RA8P1.

Codex may:
- modify source;
- analyze build logs supplied by the user;
- propose fixes;
- create host-side tests.

Codex may not assume that:
- e² studio generated successfully;
- firmware built successfully;
- flashing succeeded;
- hardware behavior is correct

unless those results have been explicitly verified.