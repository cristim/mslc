// EXPECT: error cannot be sampled with a coord::pixel sampler
// The alias keeps the sampler's unnormalised state, so the cube rule still applies through it.
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texturecube<float> t [[texture(0)]])
{
	constexpr sampler L(coord::pixel);
	sampler u = L;
	return t.sample(u, float3(0.5));
}
