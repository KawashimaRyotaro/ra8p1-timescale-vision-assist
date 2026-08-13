# System Architecture

## Target architecture

```text
Camera
  |
  v
Frame acquisition
  |
  v# System Architecture

## 1. System Overview

The system performs time-scale-aware visual processing on the Renesas
EK-RA8P1.

Visual events are separated according to temporal urgency.

- High-urgency events are processed through a low-latency fast path.
- Lower-urgency events are processed through a more detailed recognition path.

The central architectural requirement is that the detailed processing path
must not prevent the fast path from meeting its latency requirement.

The proposed system architecture is:

```text
Camera
  |
  v
Frame Acquisition
  |
  v
Temporal / Motion Analysis
  |
  +------------------------------+
  |                              |
  v                              v
Fast Path                    Detailed Path
High urgency                 Lower urgency
  |                              |
  v                              v
Fast Inference               Detailed Inference
  |                              |
  v                              v
Danger Direction             Semantic Information
  |                              |
  v                              v
Guidance Command             Audio Output
```

## 2. Role of μT-Kernel

μT-Kernel is a core part of the proposed architecture.

The fast and detailed processing paths have different real-time
requirements.

The fast path must react to urgent events with bounded latency.

The detailed path may use more computation because its deadline is less
strict.

The intended RTOS structure is conceptually:

```text
μT-Kernel

Higher priority
  |
  +-- urgent event handling
  +-- motion / urgency analysis
  +-- fast inference coordination
  +-- urgent guidance command

Lower priority
  |
  +-- detailed inference
  +-- semantic processing
  +-- non-urgent output processing
```

The exact number of tasks and their priority levels are not fixed yet.

They must be determined from implementation complexity and measured timing
behavior.

Do not create additional RTOS tasks unless there is a clear architectural or
measurement-based reason.

## 3. Processing Pipeline

### 3.1 Frame Acquisition

The camera subsystem acquires image frames and exposes them to the
application through a defined frame interface.

Responsibilities include:

- camera initialization;
- frame acquisition;
- frame-buffer management;
- acquisition timing;
- notification that a new frame is available.

Camera-specific hardware control should be separated from higher-level image
processing where practical.

### 3.2 Temporal / Motion Analysis

Temporal analysis compares image information across frames.

Its purpose is to identify regions whose temporal behavior indicates a
potentially urgent event.

Planned functions include:

- frame difference;
- changed-region detection;
- motion-related feature extraction;
- region-of-interest generation;
- urgency estimation.

This stage determines whether image information should enter the fast path,
the detailed path, or both.

The exact urgency metric is not fixed at the current development stage.

### 3.3 Fast Path

The fast path is intended for visual events that require short response time.

Its output does not necessarily require detailed semantic classification.

The minimum useful output is information such as:

- presence of an urgent event;
- approximate location;
- danger direction.

The fast path must be designed to minimize:

- blocking;
- unnecessary memory copies;
- unbounded execution;
- dependence on lower-priority processing.

### 3.4 Detailed Path

The detailed path processes visual information for which more computation is
acceptable.

Its purpose is to obtain richer semantic information such as:

- object class;
- object position;
- contextual information.

Detailed recognition must not prevent the fast path from meeting its timing
requirement.

### 3.5 Output

The system has two logically separate output channels.

Fast path:

```text
danger information
        |
        v
guidance command
```

Detailed path:

```text
semantic information
        |
        v
audio information
```

GVS is a planned guidance interface.

During firmware development, the guidance subsystem must initially be
represented by a logical command interface, GPIO/DAC/PWM output, dummy load,
or measurement setup.

Human stimulation is not required for the minimum viable prototype.

## 4. Target Hardware

Target board:

- Renesas EK-RA8P1

RTOS:

- μT-Kernel 3.0 BSP2

Current verified firmware baseline:

- official TRON Forum EK-RA8P1 μT-Kernel BSP2 sample;
- e² studio 2026-04.2;
- Renesas FSP 6.5.0.

The verified BSP baseline must remain recoverable through Git history.

## 5. Initial Processor Strategy

Development begins from the existing verified single-core BSP configuration.

The initial objective is to validate the complete processing architecture
before introducing additional processor partitioning.

Dual-core execution is therefore optional.

Possible future partitioning may use:

- Cortex-M85 for vision / inference;
- Cortex-M33 for control or other real-time functions.

However, dual-core operation must not become a dependency for the minimum
viable prototype.

Dual-core integration is considered only after the single-core processing
pipeline is stable and measured.

## 6. Firmware Organization

The existing official BSP structure is preserved.

Project-owned application code should normally be placed under:

```text
firmware/ra8p1/Application/
```

Planned logical organization is:

```text
Application/
|
+-- core/
|   +-- application coordination
|
+-- rtos/
|   +-- task definitions
|   +-- synchronization
|   +-- timing instrumentation
|
+-- vision/
|   +-- frame processing
|   +-- frame difference
|   +-- motion analysis
|   +-- ROI extraction
|
+-- inference/
|   +-- fast inference interface
|   +-- detailed inference interface
|
+-- output/
|   +-- guidance command
|   +-- audio output
|
+-- platform/
    +-- application-specific hardware abstraction
```

These directories are a target organization, not a requirement to create all
of them immediately.

They should be introduced only when the corresponding functionality is
implemented.

## 7. Vendor and Generated Code Boundary

The following areas are treated as generated or third-party code unless
there is an explicit reason to modify them:

```text
firmware/ra8p1/ra_gen/
firmware/ra8p1/ra_cfg/
firmware/ra8p1/ra/
firmware/ra8p1/mtk3_bsp2/
```

Application logic should not be inserted into generated FSP source files.

If hardware configuration must change:

1. identify the required peripheral or pin configuration;
2. determine the required e² studio / FSP Smart Configurator setting;
3. perform the configuration through the supported FSP workflow;
4. regenerate project content;
5. review the generated changes;
6. build and verify on EK-RA8P1.

Manual editing of generated FSP files is not the normal development method.

## 8. Hardware Abstraction

Algorithms that do not require RA8P1-specific hardware should be kept
independent from FSP where practical.

Examples include:

- frame-difference calculation;
- motion metrics;
- ROI generation;
- urgency decision logic;
- latency-statistics processing.

Hardware-dependent functionality includes:

- camera peripheral configuration;
- DMA;
- interrupts;
- timers;
- NPU runtime integration;
- physical output peripherals.

This separation allows selected algorithms to be tested on the host PC
without replacing hardware validation.

## 9. Memory and Real-Time Design Principles

Embedded implementation should follow these principles:

- prefer static allocation when maximum sizes are known;
- avoid unnecessary dynamic allocation;
- avoid unbounded blocking in high-priority tasks;
- minimize memory copies in image-processing paths;
- keep interrupt service routines short;
- perform heavy processing outside interrupt context;
- use explicit synchronization between processing stages;
- instrument timing before attempting premature optimization.

Optimization decisions should be based on measurements.

## 10. Primary Evaluation Architecture

The core experiment compares fast-path behavior under different system loads.

Conceptually:

```text
Case A
Fast workload only

Case B
Fast workload
+
Detailed workload
without effective priority isolation

Case C
Fast workload
+
Detailed workload
with μT-Kernel priority scheduling
```

Primary measurements include:

- end-to-end fast-path latency;
- average latency;
- P95 latency;
- worst-case latency;
- deadline miss count;
- deadline miss rate.

Secondary measurements may include:

- memory footprint;
- CPU utilization;
- NPU utilization;
- power consumption where practical.

The central evaluation question is:

> Can the high-priority fast path retain bounded response latency while the
> lower-priority detailed workload is executing?
Temporal / motion analysis
  |
  +------------------------------+
  |                              |
  v                              v
Fast path                   Detailed path
High urgency                Lower urgency
  |                              |
  v                              v
Fast inference              Detailed inference
  |                              |
  v                              v
Danger direction            Semantic information
  |                              |
  v                              v
Guidance command            Audio output