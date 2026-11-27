// RUN: grid=16 local=16 cases=3
// Two read_write 2D float textures updated in place at the thread's own texel (t % 4, t / 4), plus a read-only
// texture fetched at the mirrored coordinate (3 - x, 3 - y). Two read_write textures take different updates, so
// swapping their bindings, swapping x and y, or reading the wrong texel of the read-only texture changes a texture.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read_write> a [[texture(0)]], texture2d<float, access::read_write> b [[texture(1)]],
	texture2d<float, access::read> r [[texture(2)]], uint t [[thread_position_in_grid]]) {
	uint2 p = uint2(t % 4, t / 4);
	a.write(a.read(p) * 2.0f + r.read(uint2(3 - p.x, 3 - p.y)), p);
	b.write(b.read(p) + float4(1.0f, 2.0f, 3.0f, 4.0f), p);
}
