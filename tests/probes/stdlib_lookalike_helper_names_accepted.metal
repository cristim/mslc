// EXPECT: valid
// DISASM: OpReturnValue %float_7
// DISASM: OpReturnValue %float_8
// DISASM: OpReturnValue %float_9
// Under "using namespace metal", file-scope helpers whose names end in a library name
// (xsin, _clamp, snormalize) are their own functions and are called (Apple accepts).
#include <metal_stdlib>
using namespace metal;
float xsin(float x) { return 7.0f; }
float _clamp(float x, float lo, float hi) { return 8.0f; }
float snormalize(float3 v) { return 9.0f; }
kernel void stdlib_lookalike_helper_names_accepted(device float* out [[buffer(0)]])
{ out[0] = xsin(1.0f) + _clamp(1.5f, 0.0f, 2.0f) + snormalize(float3(1)); }
