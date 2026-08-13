# Development Roadmap

## Purpose of this roadmap

This roadmap describes the current best hypothesis for reaching
a contest-ready prototype.

It is not a fixed development sequence.

The project explicitly allows exploratory work outside this roadmap
when an experiment can improve understanding of RA8P1, edge AI,
μT-Kernel, or the proposed architecture.

Results from experiments may:
- reorder milestones;
- remove milestones;
- add milestones;
- change the system architecture;
- replace implementation technologies.

The only permanent requirement is to preserve a recoverable
known-working baseline and eventually converge toward a contest submission.

## Development Policy

Development proceeds incrementally from the already verified official
EK-RA8P1 μT-Kernel BSP2 baseline.

Each milestone must leave the repository in a known working state.



The development order is:

```text
Baseline
   |
   v
P0 RTOS/Application Foundation
   |
   v
P1 Camera Acquisition
   |
   v
P2 Temporal Analysis
   |
   v
P3 AI Inference
   |
   v
P4 Time-Scale-Aware Scheduling
   |
   v
P5 Output Integration
   |
   v
P6 Optional Optimization
```

---

## Baseline — Official BSP

### Status

Completed.

### Goal

Establish a known-working EK-RA8P1 + μT-Kernel development environment
before introducing project-specific code.

### Completed items

- e² studio installed and configured;
- Renesas FSP installed;
- official TRON Forum EK-RA8P1 μT-Kernel BSP2 project obtained;
- project imported into e² studio;
- firmware built successfully;
- firmware flashed to EK-RA8P1;
- μT-Kernel application execution confirmed;
- source imported into the project Git repository;
- baseline committed;
- baseline pushed to GitHub.

### Baseline tag

```text
baseline-ra8p1-bsp2
```

This tag is the recovery point for the unmodified known-working BSP.

---

## P0 — Application and RTOS Foundation

### Goal

Create the minimum project-owned software structure required for later
real-time experiments.

No camera or AI processing is introduced yet.

### Tasks

1. Inspect the existing BSP sample application.
2. Identify:
   - `usermain`;
   - existing task creation;
   - task priorities;
   - delay / sleep mechanisms;
   - debug output mechanisms.
3. Establish project-owned application modules under `Application/`.
4. Introduce a simple high-priority workload.
5. Introduce a simple low-priority workload.
6. Add timing instrumentation.
7. Verify that task execution and priority behavior can be observed.

### Do not add in P0

- camera capture;
- image processing;
- AI inference;
- audio;
- GVS;
- dual-core operation.

### Exit criteria

- project-owned tasks execute on EK-RA8P1;
- high- and low-priority tasks can be distinguished;
- timing data can be collected;
- e² studio build succeeds;
- hardware execution is verified;
- changes are committed and tagged.

### Planned tag

```text
p0-rtos
```

---

## P1 — Camera Acquisition

### Goal

Acquire repeatable image frames from the camera and expose them through a
stable application interface.

### Tasks

1. Identify the camera hardware interface on EK-RA8P1.
2. Determine required FSP modules and pin configuration.
3. Configure the camera through e² studio / FSP.
4. Initialize the camera sensor.
5. Configure image transfer.
6. Establish one or more frame buffers.
7. Expose frame-ready information to application code.
8. Measure frame acquisition timing.

### Questions to resolve

- camera interface;
- frame resolution;
- pixel format;
- frame-buffer location;
- DMA requirements;
- memory capacity;
- achievable frame rate.

### Exit criteria

- camera initialization succeeds;
- frames are repeatedly acquired;
- frame data format is known;
- frame acquisition timing is measured;
- buffer corruption is absent during continuous operation.

### Planned tag

```text
p1-camera
```

---

## P2 — Temporal / Motion Analysis

### Goal

Implement the time-scale-related preprocessing that determines processing
urgency.

### Initial processing candidates

- frame difference;
- thresholding;
- changed-region extraction;
- ROI generation;
- motion magnitude estimation.

### Tasks

1. Define a minimal frame representation for temporal processing.
2. Implement frame difference.
3. Extract changed regions.
4. Generate one or more ROIs.
5. Define an initial urgency metric.
6. Characterize computational cost.
7. Determine whether the algorithm can run within the required frame period.

### Design rule

Do not prematurely optimize the algorithm using RA8P1-specific instructions
until a correct reference implementation exists.

### Exit criteria

- motion-related regions can be extracted;
- the output is deterministic for known inputs;
- execution time is measured;
- application can distinguish at least two urgency classes.

### Planned tag

```text
p2-motion
```

---

## P3 — AI Inference

### Goal

Run at least one verified AI inference model on RA8P1.

### Tasks

1. Confirm the supported RA8P1 AI toolchain.
2. Select a model already compatible with the target toolchain when
   practical.
3. Document:
   - input dimensions;
   - input format;
   - quantization;
   - output tensor format.
4. Integrate inference into the application.
5. Run inference on EK-RA8P1.
6. Measure inference latency.
7. Measure memory usage where practical.

### Development rule

Do not begin by training a custom model unless the system requires one.

First establish a known-working inference path.

### Exit criteria

- one AI model executes reliably on EK-RA8P1;
- model input/output is documented;
- inference result can be consumed by application code;
- inference latency is measured.

### Planned tag

```text
p3-inference
```

---

## P4 — Time-Scale-Aware RTOS Scheduling

### Goal

Demonstrate the principal technical contribution of the project.

The fast visual-processing path must retain bounded response latency while a
lower-priority detailed workload is active.

### Architecture

```text
Temporal Analysis
       |
       +----------------------+
       |                      |
       v                      v
Fast workload            Detailed workload
High priority            Low priority
       |                      |
       v                      v
Urgent output            Semantic output
```

### Tasks

1. Integrate urgency classification with task dispatch.
2. Implement fast-path execution.
3. Implement detailed-path execution.
4. Assign μT-Kernel priorities.
5. Add end-to-end latency instrumentation.
6. Generate controlled concurrent workloads.
7. Measure scheduling behavior.

### Core experiment

Measure fast-path latency under at least:

```text
A. Fast workload only

B. Fast workload + detailed workload

C. Fast workload + detailed workload
   with intended μT-Kernel priority scheduling
```

### Primary metrics

- mean latency;
- P95 latency;
- worst-case latency;
- deadline miss count;
- deadline miss rate.

### Exit criteria

The effect of RTOS priority scheduling on fast-path real-time behavior is
quantitatively demonstrated.

### Planned tag

```text
p4-scheduler
```

---

## P5 — Output Integration

### Goal

Connect recognition results to the walking-assistance output interfaces.

### Fast-path output

Logical flow:

```text
urgent event
    |
    v
danger direction
    |
    v
guidance command
```

Initial implementation may output:

- GPIO state;
- serial command;
- DAC command;
- PWM command;
- dummy-load drive command.

Human GVS stimulation is not required for this milestone.

### Detailed-path output

Logical flow:

```text
recognition result
        |
        v
semantic information
        |
        v
audio output
```

### Tasks

1. Define a guidance-command API.
2. Map fast-path output to guidance direction.
3. Implement a safe dummy output first.
4. Define semantic-information output format.
5. Integrate the selected audio method.
6. Verify complete end-to-end operation.

### Exit criteria

A camera-derived event can propagate through processing and produce the
intended logical output.

### Planned tag

```text
p5-prototype
```

---

## P6 — Optional Optimization

### Goal

Improve performance only after the complete prototype is stable.

### Candidate work

- Cortex-M85 / Cortex-M33 partitioning;
- CPU/NPU overlap;
- memory-layout optimization;
- frame-buffer optimization;
- DMA optimization;
- reduced copies;
- power optimization;
- model optimization.

### Rule

P6 is optional.

Do not begin P6 if doing so risks destabilizing the complete P0-P5 system.

---

## Final Contest Preparation

### Goal

Convert the working prototype into a reproducible contest submission.

### Required work

- freeze functional implementation;
- clean repository structure;
- complete README;
- document environment versions;
- document build procedure;
- document flash/debug procedure;
- document hardware setup;
- document third-party software;
- document licenses;
- collect latency results;
- collect memory results;
- prepare architecture figures;
- prepare demonstration procedure;
- verify repository from a clean checkout.

### Final tag

```text
contest-submission
```

---

## Current Position

Current project state:

```text
Baseline BSP        COMPLETE
        |
        v
Documentation / Codex handoff
        |
        v
P0 RTOS Foundation  NEXT
        |
        v
P1 Camera
        |
        v
P2 Temporal Analysis
        |
        v
P3 Inference
        |
        v
P4 Scheduling
        |
        v
P5 Output
```

No P0 application changes should be made until the repository documentation
and Codex project context have been committed.