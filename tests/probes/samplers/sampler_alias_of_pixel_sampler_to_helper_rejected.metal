// EXPECT: error is a coord::pixel sampler, which a helper function does not take
#include <metal_stdlib>
using namespace metal;
float4 look(texture2d<float> t, sampler u) { return t.sample(u, float2(0.5)); }
fragment float4 f(texture2d<float> t [[texture(0)]])
{
	constexpr sampler L(coord::pixel);
	sampler u = L;
	return look(t, u);
}
