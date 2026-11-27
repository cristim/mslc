// RUN: grid=48 local=16 cases=3
// Writable and read_write non-2D textures (1D write, 1D array read_write, 2D array write, 3D read_write):
// write-only 1D and 2D array textures leave untouched texels at their seed, while 1D array and 3D update in place.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture1d<float, access::write> w1 [[texture(0)]], texture1d_array<int, access::read_write> rw1a [[texture(1)]],
	texture2d_array<uint, access::write> w2a [[texture(2)]], texture3d<short, access::read_write> rw3 [[texture(3)]],
	device uint4 *dims [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	if (i < 3u)
		w1.write(float4(float(i) + 1.0f, -float(i) - 0.5f, 2.5f, 0.25f), i);
	if (i < 12u) {
		uint lx = i & 3u, ll = i >> 2;
		rw1a.write(as_type<int4>(as_type<uint4>(rw1a.read(lx, ll)) + uint4(lx + 1u, 0u - ll - 2u, 3u, 0u - 5u)), lx, ll);
	}
	if (i < 37u) {
		uint ax = i & 3u, ay = (i >> 2) & 3u, al = i >> 4;
		w2a.write(uint4(ax + 1u, ay + 2u, al + 3u, i + 10u), uint2(ax, ay), al);
	}
	if (i < 32u) {
		uint3 p = uint3(i & 3u, (i >> 2) & 3u, i >> 4);
		rw3.write(short4(int4(rw3.read(p)) + int4(int(p.x) + 1, -int(p.y) - 1, int(p.z) + 2, -3)), p);
	}
	dims[i] = uint4(w1.get_width(), rw1a.get_array_size(), w2a.get_array_size(), rw3.get_depth());
}
