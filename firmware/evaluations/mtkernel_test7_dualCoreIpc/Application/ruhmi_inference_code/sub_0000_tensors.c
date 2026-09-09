#include "sub_0000_tensors.h"

const TensorInfo sub_0000_tensors[] = {
  { "_split_1_command_stream", 1, 876, "COMMAND_STREAM", 0xffffffff },
  { "_split_1_flash", 2, 217968, "MODEL", 0xffffffff },
  { "_split_1_scratch", 3, 768, "ARENA", 0x0 },
  { "_split_1_scratch_fast", 4, 768, "FAST_SCRATCH", 0x0 },
  { "input_1", 5, 640, "INPUT_TENSOR", 0x0 },
  { "Identity_70029", 0, 640, "OUTPUT_TENSOR", 0x80 },
};

const size_t sub_0000_tensors_count = sizeof(sub_0000_tensors) / sizeof(sub_0000_tensors[0]);

// Addresses for each input and output buffer inside of the arena
const uint32_t sub_0000_address_input_1 = 0x0;
const uint32_t sub_0000_address_Identity_70029 = 0x80;

