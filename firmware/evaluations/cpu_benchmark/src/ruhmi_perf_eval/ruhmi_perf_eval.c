/*
 * EK-RA8P1 CPU benchmark for the MLCommons Tiny ad01_int8 model.
 * The measured NPU result is embedded only as a comparison baseline.
 */
#include "hal_data.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "ruhmi_inference_code/compute_sub_0000.h"
#include "ruhmi_inference_code/model_io_data.h"
#include "utils/time_counter.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>

#define CPU_BENCHMARK_WARMUP_RUNS                 (1U)
#define CPU_BENCHMARK_MEASURED_RUNS               (100U)

/* Measured by the matching NPU project on this EK-RA8P1 at a 1 GHz CPU clock. */
#define NPU_REFERENCE_CPU_CYCLES_AVERAGE          (107680U)

typedef enum e_cpu_benchmark_status
{
    CPU_BENCHMARK_NOT_RUN = 0,
    CPU_BENCHMARK_RUNNING,
    CPU_BENCHMARK_PASSED,
    CPU_BENCHMARK_OUTPUT_MISMATCH
} cpu_benchmark_status_t;

typedef struct st_cpu_benchmark_result
{
    cpu_benchmark_status_t status;
    uint32_t               warmup_runs;
    uint32_t               measured_runs;
    uint32_t               cpu_clock_hz;
    uint32_t               cpu_cycles_first;
    uint32_t               cpu_cycles_min;
    uint32_t               cpu_cycles_max;
    uint32_t               cpu_cycles_average;
    uint32_t               inference_time_average_us;
    uint32_t               npu_reference_cycles_average;
    uint32_t               npu_speedup_x1000;
    uint32_t               output_mismatches;
    int32_t                output_max_abs_error;
} cpu_benchmark_result_t;

volatile cpu_benchmark_result_t g_cpu_benchmark_result;

static uint8_t g_compute_storage[kBufferSize_sub_0000] __attribute__((aligned(16)));
static int8_t  g_cpu_output[model_Identity_COUNT] __attribute__((aligned(16)));

static uint32_t benchmark_run_once(void);
static void     benchmark_compare_output(void);
static void     benchmark_print_result(void);

void ruhmi_perf_eval(void)
{
    uint64_t cpu_cycles_total = 0U;
    uint32_t cpu_cycles_min   = UINT32_MAX;
    uint32_t cpu_cycles_max   = 0U;

    memset((void *) &g_cpu_benchmark_result, 0, sizeof(g_cpu_benchmark_result));
    g_cpu_benchmark_result.status                       = CPU_BENCHMARK_RUNNING;
    g_cpu_benchmark_result.warmup_runs                  = CPU_BENCHMARK_WARMUP_RUNS;
    g_cpu_benchmark_result.measured_runs                = CPU_BENCHMARK_MEASURED_RUNS;
    g_cpu_benchmark_result.npu_reference_cycles_average = NPU_REFERENCE_CPU_CYCLES_AVERAGE;

    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "\r\nEK-RA8P1 CPU benchmark\r\n");
    SEGGER_RTT_WriteString(0, "Model: MLCommons Tiny ad01_int8\r\n");

    TimeCounter_Init();
    g_cpu_benchmark_result.cpu_clock_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_CPUCLK);

    for (uint32_t i = 0U; i < CPU_BENCHMARK_WARMUP_RUNS; i++)
    {
        compute_sub_0000(g_compute_storage, model_input_1, g_cpu_output);
    }

    for (uint32_t i = 0U; i < CPU_BENCHMARK_MEASURED_RUNS; i++)
    {
        uint32_t const cpu_cycles = benchmark_run_once();

        if (0U == i)
        {
            g_cpu_benchmark_result.cpu_cycles_first = cpu_cycles;
        }
        if (cpu_cycles < cpu_cycles_min)
        {
            cpu_cycles_min = cpu_cycles;
        }
        if (cpu_cycles > cpu_cycles_max)
        {
            cpu_cycles_max = cpu_cycles;
        }

        cpu_cycles_total += cpu_cycles;
    }

    g_cpu_benchmark_result.cpu_cycles_min     = cpu_cycles_min;
    g_cpu_benchmark_result.cpu_cycles_max     = cpu_cycles_max;
    g_cpu_benchmark_result.cpu_cycles_average =
        (uint32_t) (cpu_cycles_total / CPU_BENCHMARK_MEASURED_RUNS);
    g_cpu_benchmark_result.inference_time_average_us =
        (uint32_t) ((cpu_cycles_total * 1000000ULL) /
                    ((uint64_t) g_cpu_benchmark_result.cpu_clock_hz * CPU_BENCHMARK_MEASURED_RUNS));
    g_cpu_benchmark_result.npu_speedup_x1000 =
        (uint32_t) ((cpu_cycles_total * 1000ULL) /
                    ((uint64_t) NPU_REFERENCE_CPU_CYCLES_AVERAGE * CPU_BENCHMARK_MEASURED_RUNS));

    benchmark_compare_output();
    g_cpu_benchmark_result.status =
        (0U == g_cpu_benchmark_result.output_mismatches) ?
        CPU_BENCHMARK_PASSED : CPU_BENCHMARK_OUTPUT_MISMATCH;

    benchmark_print_result();

    /* Intentional stop: inspect g_cpu_benchmark_result in e2 studio. */
    __BKPT(0);
}

static uint32_t benchmark_run_once(void)
{
    uint32_t const start = TimeCounter_CurrentCountGet();
    compute_sub_0000(g_compute_storage, model_input_1, g_cpu_output);
    return TimeCounter_CurrentCountGet() - start;
}

static void benchmark_compare_output(void)
{
    int32_t max_abs_error = 0;
    uint32_t mismatches   = 0U;

    for (uint32_t i = 0U; i < model_Identity_COUNT; i++)
    {
        int32_t error = (int32_t) g_cpu_output[i] - (int32_t) model_Identity[i];
        if (error < 0)
        {
            error = -error;
        }
        if (error > max_abs_error)
        {
            max_abs_error = error;
        }
        if (0 != error)
        {
            mismatches++;
        }
    }

    g_cpu_benchmark_result.output_mismatches    = mismatches;
    g_cpu_benchmark_result.output_max_abs_error = max_abs_error;
}

static void benchmark_print_result(void)
{
    SEGGER_RTT_printf(0, "Status: %u (2=PASS)\r\n", (unsigned) g_cpu_benchmark_result.status);
    SEGGER_RTT_printf(0, "Runs: %u warm-up + %u measured\r\n",
                      (unsigned) g_cpu_benchmark_result.warmup_runs,
                      (unsigned) g_cpu_benchmark_result.measured_runs);
    SEGGER_RTT_printf(0, "CPU cycles: first=%u min=%u avg=%u max=%u\r\n",
                      (unsigned) g_cpu_benchmark_result.cpu_cycles_first,
                      (unsigned) g_cpu_benchmark_result.cpu_cycles_min,
                      (unsigned) g_cpu_benchmark_result.cpu_cycles_average,
                      (unsigned) g_cpu_benchmark_result.cpu_cycles_max);
    SEGGER_RTT_printf(0, "Average CPU inference time: %u us\r\n",
                      (unsigned) g_cpu_benchmark_result.inference_time_average_us);
    SEGGER_RTT_printf(0, "NPU speedup: %u.%03ux\r\n",
                      (unsigned) (g_cpu_benchmark_result.npu_speedup_x1000 / 1000U),
                      (unsigned) (g_cpu_benchmark_result.npu_speedup_x1000 % 1000U));
    SEGGER_RTT_printf(0, "Output: mismatches=%u/640 max_abs_error=%d\r\n",
                      (unsigned) g_cpu_benchmark_result.output_mismatches,
                      (int) g_cpu_benchmark_result.output_max_abs_error);
}

void ruhmi_external_memory_init(void)
{
    /* This model uses on-chip flash and SRAM only. */
}
