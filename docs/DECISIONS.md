# Architecture Decisions

## Decision policy

Architecture decisions record current reasoning, not permanent rules.

Decisions may be:
- Accepted
- Experimental
- Rejected
- Superseded

New experimental evidence may overturn previous decisions.

When a decision changes, preserve the old entry and add the reason
for the new decision rather than deleting the history.

## D001 — GitHub repository is the project system of record

Status: Accepted

The public GitHub repository contains project-owned source code,
documentation, build metadata, and development history.

## D002 — Preserve official BSP baseline

Status: Accepted

The verified TRON Forum EK-RA8P1 μT-Kernel BSP2 baseline must remain
recoverable through Git history and the baseline tag.

## D003 — e² studio is the embedded build authority

Status: Accepted

e² studio is used for:
- FSP configuration;
- firmware build;
- flashing;
- hardware debugging.

VS Code/Codex does not maintain an independent embedded build
configuration.

## D004 — VS Code + Codex is the primary implementation interface

Status: Accepted

VS Code is used for source editing, code inspection, documentation,
Git operations, and host-side development.

## D005 — Generated FSP code is not manually maintained

Status: Accepted

Peripheral configuration changes should be performed through the
supported FSP/e² studio workflow.

## D006 — Application logic remains separate from vendor code

Status: Accepted

Project-owned logic should normally reside under
`firmware/ra8p1/Application/`.

## D007 — Single-core prototype comes first

Status: Accepted

Dual-core RA8P1 operation is not required before the basic processing
pipeline has been demonstrated.

## D008 — RTOS scheduling is a core technical contribution

Status: Accepted

The high-urgency path must be protected from interference caused by
lower-priority detailed processing.

## D009 — Human GVS stimulation is not an MVP dependency

Status: Accepted

Initial validation uses a logical command interface, dummy load,
or measurement output.