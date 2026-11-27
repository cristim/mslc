// EXPECT: valid
// REFLECT-NOT: embedded_sampler": 0
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
struct Lookup { float x; float4 look(texture2d<float> t) const { return t.sample(S, float2(x)); } };
fragment float4 f() { return float4(1.0); }
