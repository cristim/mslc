// EXPECT: error which an instance does not reach
// Refs #341: the scan that reserves a file-scope sampler for an entry point runs before any instance
// exists, so an instance that names one is refused; pass the sampler to it.
#include <metal_stdlib>
using namespace metal;
constexpr sampler smp(coord::normalized, filter::linear);
template <typename T> float4 f(texture2d<float> t, T x) { return t.sample(smp, float2(x)); }
kernel void k(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]]) { out[0] = f(t, 0.5f); }
