// EXPECT: valid
// A fragment function that does not call get_num_samples() has no sample count constant to set.
// DISASM-NOT: SpecId 262147
// REFLECT-NOT: num_samples_spec_id
#include <metal_stdlib>
using namespace metal;
fragment float4 get_num_samples_not_read() { return float4(1.0f); }
