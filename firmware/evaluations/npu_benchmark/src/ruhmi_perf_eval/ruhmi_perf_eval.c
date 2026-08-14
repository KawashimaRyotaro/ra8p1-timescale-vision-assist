/*
 * EK-RA8P1 Ethos-U55 NPU benchmark.
 *
 * The model and reference vectors are generated from the MLCommons Tiny
 * ad01_int8.tflite anomaly-detection model by RUHMI 2.6.0.
 */
#include "hal_data.h"
#include "pmu_ethosu.h"
#include "SEGGER_RTT/SEGGER_RTT.h"
#include "ruhmi_inference_code/model.h"
#include "ruhmi_inference_code/model_io_data.h"
#include "utils/external_memory.h"
#include "utils/time_counter.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define NPU_BENCHMARK_WARMUP_RUNS       (1U)
#define NPU_BENCHMARK_MEASURED_RUNS     (100U)

/* The generated model keeps weights in on-chip flash and its arena in SRAM. */
#define EXTERNAL_MEMORY_OSPI_ENABLE     (0)
#define EXTERNAL_MEMORY_SDRAM_ENABLE    (0)
#define INTERNAL_MEMORY_SIP_ENABLE      (0)

typedef enum e_npu_benchmark_status
{
    NPU_BENCHMARK_NOT_RUN = 0,
    NPU_BENCHMARK_RUNNING,
    NPU_BENCHMARK_PASSED,
    NPU_BENCHMARK_OUTPUT_MISMATCH,
    NPU_BENCHMARK_NPU_OPEN_FAILED,
    NPU_BENCHMARK_NPU_CLOSE_FAILED
} npu_benchmark_status_t;

typedef struct st_npu_benchmark_result
{
    npu_benchmark_status_t status;
    uint32_t               warmup_runs;
    uint32_t               measured_runs;
    uint32_t               cpu_clock_hz;
    uint32_t               cpu_cycles_first;
    uint32_t               cpu_cycles_min;
    uint32_t               cpu_cycles_max;
    uint32_t               cpu_cycles_average;
    uint32_t               inference_time_average_us;
    uint32_t               npu_active_cycles_average;
    uint32_t               output_mismatches;
    int32_t                output_max_abs_error;
} npu_benchmark_result_t;

/* Inspect this object in the e2 studio Expressions view after the final breakpoint. */
volatile npu_benchmark_result_t g_npu_benchmark_result;

static uint32_t benchmark_run_once(void);
static void     benchmark_compare_output(void);
static void     benchmark_print_result(void);

void ruhmi_perf_eval(void)
{
    uint64_t cpu_cycles_total = 0U;
    uint64_t npu_cycles_total = 0U;
    uint32_t cpu_cycles_min   = UINT32_MAX;
    uint32_t cpu_cycles_max   = 0U;

    memset((void *) &g_npu_benchmark_result, 0, sizeof(g_npu_benchmark_result));
    g_npu_benchmark_result.status        = NPU_BENCHMARK_RUNNING;
    g_npu_benchmark_result.warmup_runs   = NPU_BENCHMARK_WARMUP_RUNS;
    g_npu_benchmark_result.measured_runs = NPU_BENCHMARK_MEASURED_RUNS;

    SEGGER_RTT_Init();
    SEGGER_RTT_WriteString(0, "\r\nEK-RA8P1 Ethos-U55 NPU benchmark\r\n");
    SEGGER_RTT_WriteString(0, "Model: MLCommons Tiny ad01_int8\r\n");

    TimeCounter_Init();
    g_npu_benchmark_result.cpu_clock_hz = R_FSP_SystemClockHzGet(FSP_PRIV_CLOCK_CPUCLK);

    if (FSP_SUCCESS != RM_ETHOSU_Open(&g_rm_ethosu0_ctrl, &g_rm_ethosu0_cfg))
    {
        g_npu_benchmark_result.status = NPU_BENCHMARK_NPU_OPEN_FAILED;
        SEGGER_RTT_WriteString(0, "ERROR: RM_ETHOSU_Open failed\r\n");
        __BKPT(0);
        return;
    }

    ETHOSU_PMU_Enable(&g_ethosu0);
    ETHOSU_PMU_Set_EVTYPER(&g_ethosu0, 0U, ETHOSU_PMU_NPU_ACTIVE);
    ETHOSU_PMU_CNTR_Enable(&g_ethosu0, 1UL << 0U);

    for (uint32_t i = 0U; i < NPU_BENCHMARK_WARMUP_RUNS; i++)
    {
        memcpy(GetModelInputPtr_input_1(), model_input_1, model_input_1_SIZE);
        RunModel(false);
    }

    for (uint32_t i = 0U; i < NPU_BENCHMARK_MEASURED_RUNS; i++)
    {
        memcpy(GetModelInputPtr_input_1(), model_input_1, model_input_1_SIZE);
        ETHOSU_PMU_EVCNTR_ALL_Reset(&g_ethosu0);

        uint32_t const cpu_cycles = benchmark_run_once();
        uint32_t const npu_cycles = ETHOSU_PMU_Get_EVCNTR(&g_ethosu0, 0U);

        if (0U == i)
        {
            g_npu_benchmark_result.cpu_cycles_first = cpu_cycles;
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
        npu_cycles_total += npu_cycles;
    }

    g_npu_benchmark_result.cpu_cycles_min             = cpu_cycles_min;
    g_npu_benchmark_result.cpu_cycles_max             = cpu_cycles_max;
    g_npu_benchmark_result.cpu_cycles_average         =
        (uint32_t) (cpu_cycles_total / NPU_BENCHMARK_MEASURED_RUNS);
    g_npu_benchmark_result.npu_active_cycles_average  =
        (uint32_t) (npu_cycles_total / NPU_BENCHMARK_MEASURED_RUNS);
    g_npu_benchmark_result.inference_time_average_us  =
        (uint32_t) ((cpu_cycles_total * 1000000ULL) /
                    ((uint64_t) g_npu_benchmark_result.cpu_clock_hz * NPU_BENCHMARK_MEASURED_RUNS));

    benchmark_compare_output();
    g_npu_benchmark_result.status =
        (0U == g_npu_benchmark_result.output_mismatches) ?
        NPU_BENCHMARK_PASSED : NPU_BENCHMARK_OUTPUT_MISMATCH;

    ETHOSU_PMU_Disable(&g_ethosu0);
    if (FSP_SUCCESS != RM_ETHOSU_Close(&g_rm_ethosu0_ctrl))
    {
        g_npu_benchmark_result.status = NPU_BENCHMARK_NPU_CLOSE_FAILED;
    }

    benchmark_print_result();

    /* Intentional stop: inspect g_npu_benchmark_result in e2 studio. */
    __BKPT(0);
}

static uint32_t benchmark_run_once(void)
{
    uint32_t const start = TimeCounter_CurrentCountGet();
    RunModel(false);
    return TimeCounter_CurrentCountGet() - start;
}

static void benchmark_compare_output(void)
{
    int8_t const * const actual = GetModelOutputPtr_Identity_70029();
    int32_t max_abs_error = 0;
    uint32_t mismatches   = 0U;

    for (uint32_t i = 0U; i < model_Identity_COUNT; i++)
    {
        int32_t error = (int32_t) actual[i] - (int32_t) model_Identity[i];
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

    g_npu_benchmark_result.output_mismatches  = mismatches;
    g_npu_benchmark_result.output_max_abs_error = max_abs_error;
}

static void benchmark_print_result(void)
{
    SEGGER_RTT_printf(0, "Status: %u (2=PASS)\r\n", (unsigned) g_npu_benchmark_result.status);
    SEGGER_RTT_printf(0, "Runs: %u warm-up + %u measured\r\n",
                      (unsigned) g_npu_benchmark_result.warmup_runs,
                      (unsigned) g_npu_benchmark_result.measured_runs);
    SEGGER_RTT_printf(0, "CPU cycles: first=%u min=%u avg=%u max=%u\r\n",
                      (unsigned) g_npu_benchmark_result.cpu_cycles_first,
                      (unsigned) g_npu_benchmark_result.cpu_cycles_min,
                      (unsigned) g_npu_benchmark_result.cpu_cycles_average,
                      (unsigned) g_npu_benchmark_result.cpu_cycles_max);
    SEGGER_RTT_printf(0, "Average inference time: %u us\r\n",
                      (unsigned) g_npu_benchmark_result.inference_time_average_us);
    SEGGER_RTT_printf(0, "Average NPU active cycles: %u\r\n",
                      (unsigned) g_npu_benchmark_result.npu_active_cycles_average);
    SEGGER_RTT_printf(0, "Output: mismatches=%u/640 max_abs_error=%d\r\n",
                      (unsigned) g_npu_benchmark_result.output_mismatches,
                      (int) g_npu_benchmark_result.output_max_abs_error);
}

void ruhmi_external_memory_init(void)
{
#if (EXTERNAL_MEMORY_OSPI_ENABLE == 1)
    user_ospi_b_init((void *) &g_ospi0);
#endif

#if (EXTERNAL_MEMORY_SDRAM_ENABLE == 1) && (BSP_CFG_SDRAM_ENABLED != 1)
 #error "Enable SDRAM in the FSP BSP configuration."
#endif

#if (INTERNAL_MEMORY_SIP_ENABLE == 1)
 #error "SiP memory is not used by this benchmark."
#endif
}
