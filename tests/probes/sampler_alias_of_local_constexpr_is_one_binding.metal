// EXPECT: valid
// A copy of a constexpr sampler local shares its embedded sampler: one reflection entry.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "L" }
// REFLECT-NOT: "binding": 2 }
#include <metal_stdlib>
using namespace metal;
fragment float4 f(texture2d<float> t [[texture(0)]])
{
	constexpr sampler L(filter::linear);
	sampler u = L;
	return t.sample(u, float2(0.5));
}
