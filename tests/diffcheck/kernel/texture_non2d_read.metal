// RUN: grid=48 local=16 cases=3
// Non-2D textures (1D sampled, 1D array read, 2D array read, 3D read) across float, int, uint and half, plus
// width, height, depth and array-size queries. No division on half keeps the comparison bit-exact.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float, access::sample> s1 [[texture(0)]], texture1d_array<int, access::read> r1a [[texture(1)]],
	texture2d_array<uint, access::read> r2a [[texture(2)]], texture3d<half, access::read> r3 [[texture(3)]],
	sampler smp [[sampler(0)]], device float4 *of [[buffer(0)]], device int4 *oi [[buffer(1)]],
	device uint4 *ou [[buffer(2)]], device half4 *oh [[buffer(3)]], device uint4 *dims [[buffer(4)]],
	uint i [[thread_position_in_grid]]) {
	uint x = i & 3u, y = (i >> 2) & 3u, z = (i >> 4) & 1u, lay = (i >> 4) % 3u;
	of[i] = s1.sample(smp, (float(x) + 0.5f) * 0.25f);
	oi[i] = r1a.read(3u - x, lay);
	ou[i] = r2a.read(uint2(y, x), 2u - lay);
	oh[i] = r3.read(uint3(x, 3u - y, z));
	dims[i] = uint4(s1.get_width() + r2a.get_height(), r1a.get_array_size(), r2a.get_array_size(), r3.get_depth());
}
