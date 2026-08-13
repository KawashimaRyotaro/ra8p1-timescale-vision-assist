# Project Context

## Project name

Time-Scale-Adaptive Vision System for Walking Assistance

Japanese title:

時間スケール適応型視覚認識による歩行支援システム

## Contest

TRON Programming Contest 2026

Target platform:
- Renesas EK-RA8P1
- μT-Kernel 3.0 BSP2

## Problem definition

Visual information does not always require the same processing latency
or the same recognition detail.

Objects or events associated with short time-to-collision require a
fast response, while lower-urgency objects can use more computationally
expensive recognition.

Processing all visual information using one large recognition pipeline
creates an avoidable trade-off between latency and recognition detail.

## Proposed principle

The system separates visual processing according to temporal urgency.

### Fast path

Purpose:
- detect urgent visual events;
- determine dangerous direction;
- minimize response latency.

This path receives high RTOS priority.

### Detailed path

Purpose:
- recognize lower-urgency objects in greater semantic detail;
- provide environmental information to the user.

This path receives lower RTOS priority.

## Role of μT-Kernel

μT-Kernel is not used only as a software framework.

Its task priority mechanism is part of the proposed architecture.

The central systems question is:

Can the high-priority fast path retain bounded response latency while
the lower-priority detailed recognition workload is executing?

## Output concept

Fast path:

danger direction
-> guidance command

Detailed path:

object information
-> audio information

GVS is planned as a possible guidance interface.

Initial firmware validation will use a command abstraction or dummy
electrical output rather than human stimulation.

## Primary evaluation metrics

- fast-path end-to-end latency
- P95 latency
- worst-case latency
- deadline miss count
- deadline miss rate
- memory footprint
- CPU/NPU utilization where measurable

## Non-goals

The primary contribution is not:

- maximizing object-detection benchmark accuracy;
- creating a new generic vision model;
- modifying the μT-Kernel core;
- demonstrating RA8P1 dual-core operation merely for complexity.

AI accuracy matters only insofar as it supports the complete system.