// EXPECT: valid
// A kernel gathers too (a gather has no lod); the result of a half texture is narrowed.
// DISASM: OpImageGather
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d<half> a [[texture(0)]], sampler s [[sampler(0)]], device half4* o [[buffer(0)]]) {
  o[0] = a.gather(s, float2(0.5), int2(1, 0), component::w);
}
