// EXPECT: valid
// A depth texture is its own helper parameter type: a texture2d<float> argument for a depth2d<float> parameter is refused.
#include <metal_stdlib>
using namespace metal;
// DISASM: OpFunctionCall
float depthAt(depth2d<float> d, sampler s, float2 p) { return d.sample(s, p); }
fragment float4 hit(depth2d<float> d [[texture(0)]], sampler s [[sampler(0)]]) { return float4(depthAt(d, s, float2(0.5))); }
