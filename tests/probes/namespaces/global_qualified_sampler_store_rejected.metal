// EXPECT: error cannot be assigned
#include <metal_stdlib>
using namespace metal;
constexpr sampler s(filter::linear);
kernel void kern(device float4* out [[buffer(0)]], texture2d<float> t [[texture(0)]]) {
	float s = 1.0f;
	::s = sampler(filter::nearest);
	out[0] = t.sample(s, float2(0.5f));
}
