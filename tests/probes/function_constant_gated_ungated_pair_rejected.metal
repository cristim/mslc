// EXPECT: error the texture index 0 is used by more than one parameter of
// Two ungated parameters may not share an index even beside a gated one; Apple's compiler cannot reserve the location.
#include <metal_stdlib>
using namespace metal;
constant bool g [[function_constant(0)]];
kernel void k(device float* out [[buffer(0)]], texture2d<float> a [[texture(0)]], texture2d<float> b [[texture(0)]],
              texture2d_array<float> c [[texture(0), function_constant(g)]]) { out[0] = 1; }
