// RUN: grid=64 local=16 cases=3
// Non-float 2D textures (int, uint, half) read texel by texel with access::read, plus a sampled ushort 2D texture
// sampled at texel centres with a nearest sampler. Different coordinate permutations per texture ensure swapped
// coordinates, swapped bindings, or a wrong element type change the output.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<int, access::read> ti [[texture(0)]], texture2d<uint, access::read> tu [[texture(1)]],
	texture2d<half, access::read> th [[texture(2)]], texture2d<ushort, access::sample> tus [[texture(3)]],
	sampler s [[sampler(0)]], device int4 *oi [[buffer(0)]], device uint4 *ou [[buffer(1)]],
	device half4 *oh [[buffer(2)]], device ushort4 *ous [[buffer(3)]], uint i [[thread_position_in_grid]]) {
	uint x = i % 4, y = i / 4 % 4;
	oi[i] = ti.read(uint2(x, y));
	ou[i] = tu.read(uint2(3 - x, y));
	oh[i] = th.read(uint2(x, 3 - y));
	ous[i] = tus.sample(s, (float2(3 - x, 3 - y) + 0.5f) / 4.0f);
}
