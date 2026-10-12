// EXPECT: valid
// DISASM: OpConvertUToF %float
// DISASM-NOT: OpConvertSToF
//
// The layer of sample is a uint in Metal, so an int layer becomes a uint
// before the float coordinate takes it: -1 wraps rather than clamping to 0.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d_array<float> t [[texture(0)]], sampler s [[sampler(0)]],
                  constant int *layer [[buffer(0)]]) {
  return t.sample(s, float2(0.5), layer[0]);
}
