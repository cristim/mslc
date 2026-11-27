// EXPECT: error helper function "pack_float_to_unorm4x8" has the name of a builtin function
// A function of the file with the name of a <metal_pack> function competes with the library's for a call,
// which Apple resolves by overload (an exact match for both is ambiguous); mslc refuses it, as it does a
// helper named like a math builtin, rather than call the wrong one.
#include <metal_stdlib>
using namespace metal;
static uint pack_float_to_unorm4x8(float4 v) { return uint(v.x) + 1u; }
kernel void k(device const float4* in [[buffer(0)]], device uint* out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
  out[i] = pack_float_to_unorm4x8(in[i]);
}
