// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// powr(x, y) is x to the power y where x >= 0 (MSL 6.6), so GLSL.std.450 Pow, which is undefined for x < 0
// as powr is. A scalar beside a vector is broadcast, as for pow.
// DISASM-MATCH: OpExtInst %float %[0-9]+ Pow %
// DISASM-MATCH: OpExtInst %v3float %[0-9]+ Pow %
// DISASM-MATCH: OpExtInst %half %[0-9]+ Pow %
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	const float x = float(i) + 1.0f;
	const float3 v = powr(float3(x, 2.0f, 4.0f), 0.5f);
	out[i] = powr(x, 2.0f) + v.x * 10.0f + float(powr(half(x), half(3.0f))) * 100.0f + metal::powr(x, 1.0f) * 1000.0f;
}
