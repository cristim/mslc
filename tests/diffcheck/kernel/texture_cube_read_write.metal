// RUN: grid=96 local=16 cases=3
// Cube and cube_array textures across sample, write and read_write: threads visit all 6 faces and both layers of
// the cube array, while face 5 of the write-only half cube stays at its seed.
#include <metal_stdlib>
using namespace metal;
kernel void k(texturecube<float, access::sample> sc [[texture(0)]], texturecube_array<float, access::sample> sca [[texture(1)]],
	texturecube<half, access::write> wc [[texture(2)]], texturecube_array<uint, access::read_write> rwca [[texture(3)]],
	sampler smp [[sampler(0)]], device float4 *outC [[buffer(0)]], device float4 *outCA [[buffer(1)]],
	device uint4 *dims [[buffer(2)]], uint i [[thread_position_in_grid]]) {
	uint x = i & 3u, y = (i >> 2) & 3u, face = i >> 4;
	float u = (float(x) + 0.5f - 2.0f) * 0.5f;
	float v = (float(y) + 0.5f - 2.0f) * 0.5f;
	float3 dir = face == 0u ? float3(1.0f, -v, -u)
		: face == 1u ? float3(-1.0f, -v, u)
		: face == 2u ? float3(u, 1.0f, v)
		: face == 3u ? float3(u, -1.0f, -v)
		: face == 4u ? float3(u, -v, 1.0f)
		: float3(-u, -v, -1.0f);
	outC[i] = sc.sample(smp, dir);
	outCA[i] = sca.sample(smp, dir, face & 1u);
	if (face < 5u)
		wc.write(half4(half(x) + 0.5h, half(y) - 0.5h, half(face), 1.0h), uint2(x, y), face);
	for (uint layer = 0u; layer < 2u; layer++)
		rwca.write(rwca.read(uint2(x, y), face, layer) + uint4(x + 1u, y + 2u, face + 3u, layer + 4u), uint2(x, y), face, layer);
	dims[i] = uint4(sc.get_width(), wc.get_height(), sca.get_array_size(), rwca.get_array_size());
}
