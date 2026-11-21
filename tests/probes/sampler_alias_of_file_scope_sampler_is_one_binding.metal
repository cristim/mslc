// EXPECT: valid
// A local copy of a file-scope sampler names it, so the sampler is reserved once, at the one binding.
// REFLECT: { "kind": "Sampler", "descriptor": { "set": 1, "binding": 1 }, "embedded_sampler": 0, "name": "S" }
// REFLECT-NOT: "binding": 2 }
// DISASM-NO-MATCH: Binding 2
#include <metal_stdlib>
using namespace metal;
constexpr sampler S(filter::linear);
fragment float4 f(texture2d<float> t [[texture(0)]])
{
	sampler u = S;
	auto v = u;
	return t.sample(v, float2(0.5));
}
