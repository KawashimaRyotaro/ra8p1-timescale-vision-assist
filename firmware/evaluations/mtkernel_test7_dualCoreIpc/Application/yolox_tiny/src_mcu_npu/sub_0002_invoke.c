#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <tk/tkernel.h>

#include "common_data.h"

#include "sub_0002_tensors.h"
#include "sub_0002_command_stream.h"
#include "sub_0002_model_data.h"

#include "sub_0002_invoke.h"

// Include Ethos-U driver headers (Assumed to be available)
#include "ethosu_driver.h"

// Define arenas with allocation and 16-byte alignment
__attribute__((section(".sdram"), aligned(16))) uint8_t sub_0002_arena[802816];
// Fast scratch arena not used for Ethos-U55
//  We will not create it for now and reuse the address of the other arena
// __attribute__((aligned(16))) static uint8_t sub_0002_fast_scratch[802816];
uint8_t* sub_0002_fast_scratch = sub_0002_arena;

volatile uint32_t g_ethosu_invoke_count    = 0U;
volatile uint32_t g_ethosu_invoke_last_ms  = 0U;
volatile uint32_t g_ethosu_invoke_total_ms = 0U;
volatile uint32_t g_ethosu_invoke_min_ms   = UINT32_MAX;
volatile uint32_t g_ethosu_invoke_max_ms   = 0U;

volatile int g_ethosu_invoke_last_result = 0;


static uint8_t sub_0002_get_time_ms(uint64_t * time_ms)
{
    if (NULL == time_ms)
    {
        return 0U;
    }

    SYSTIM system_time;

    ER err = tk_get_otm(&system_time);

    if (err < E_OK)
    {
        return 0U;
    }

    *time_ms =
        ((uint64_t) (uint32_t) system_time.hi << 32) |
        (uint64_t) system_time.lo;

    return 1U;
}

int sub_0002_invoke(bool clean_outputs) {
  // Initialize base addresses and sizes
  uint64_t base_addrs[8] = {0};
  size_t base_addrs_size[8] = {0};
  int num_base_addrs = 8;

  // Variables for command stream
  uint8_t* cms_data = NULL;
  int cms_size = 0;

  // Prepare base_addrs and base_addrs_size arrays
  // Buffer sub_0002_model with size 4670848 and address: 4294967295
  base_addrs[0] = (uint64_t)(uintptr_t)sub_0002_model_data;
  base_addrs_size[0] = sub_0002_model_data_size;
  // Buffer sub_0002_arena with size 802816 and address: 0
  base_addrs[1] = (uint64_t)(uintptr_t) (sub_0002_arena+0);
  base_addrs_size[1] = 802816;

  // Buffer sub_0002_fast_scratch with size 802816 and address: 0
  base_addrs[2] = (uint64_t)(uintptr_t) (sub_0002_arena+0);
  base_addrs_size[2] = 802816;

  // Buffer input_tensor_0 with size 37632 and address: 112896
  base_addrs[3] = (uint64_t)(uintptr_t) (sub_0002_arena+112896);
  base_addrs_size[3] = 37632;

  // Buffer input_tensor_1 with size 37632 and address: 75264
  base_addrs[4] = (uint64_t)(uintptr_t) (sub_0002_arena+75264);
  base_addrs_size[4] = 37632;

  // Buffer input_tensor_2 with size 37632 and address: 37632
  base_addrs[5] = (uint64_t)(uintptr_t) (sub_0002_arena+37632);
  base_addrs_size[5] = 37632;

  // Buffer input_tensor_3 with size 37632 and address: 0
  base_addrs[6] = (uint64_t)(uintptr_t) (sub_0002_arena+0);
  base_addrs_size[6] = 37632;

  // Buffer output_tensor_0 with size 87465 and address: 0
  if (clean_outputs) {
    memset(sub_0002_arena + 0, 0, 87465);
  }
  base_addrs[7] = (uint64_t)(uintptr_t) (sub_0002_arena+0);
  base_addrs_size[7] = 87465;

  // Command stream data
  cms_data = (uint8_t*)sub_0002_command_stream;
  cms_size = (int) sub_0002_command_stream_size;

  // Invoke the Ethos-U driver
  if (num_base_addrs > 8) {
    num_base_addrs = 8;
  }
  
  uint64_t invoke_start_ms = 0U;
  uint64_t invoke_end_ms   = 0U;

  uint8_t const timing_valid =
      sub_0002_get_time_ms(&invoke_start_ms);


  int result = ethosu_invoke_v3(
      &g_ethosu0,
      cms_data,
      cms_size,
      base_addrs,
      base_addrs_size,
      num_base_addrs,
      NULL
  );


  g_ethosu_invoke_last_result = result;


  if ((0U != timing_valid) &&
      (0U != sub_0002_get_time_ms(&invoke_end_ms)))
  {
      uint32_t const elapsed_ms =
          (uint32_t)
          (
              invoke_end_ms -
              invoke_start_ms
          );

      g_ethosu_invoke_last_ms =
          elapsed_ms;

      g_ethosu_invoke_total_ms +=
          elapsed_ms;

      g_ethosu_invoke_count++;

      if (elapsed_ms <
          g_ethosu_invoke_min_ms)
      {
          g_ethosu_invoke_min_ms =
              elapsed_ms;
      }

      if (elapsed_ms >
          g_ethosu_invoke_max_ms)
      {
          g_ethosu_invoke_max_ms =
              elapsed_ms;
      }
  }

  if (result == -1) {
    // Ethos-U invocation failed
    return -1;
  }

  return 0;
}
