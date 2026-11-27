// RUN: grid=16 local=16 cases=3
// Writable and read_write non-float 2D textures (int, uint, half, short): threads 0..10 write 11 of the 16 texels of
// the write-only int and half textures (leaving 5 at their seed), and all 16 threads update the read_write uint and
// short textures in place.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<int, access::write> wi [[texture(0)]], texture2d<half, access::write> wh [[texture(1)]],
	texture2d<uint, access::read_write> rwu [[texture(2)]], texture2d<short, access::read_write> rws [[texture(3)]],
	device uint2 *dims [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	if (t < 11) {
		uint idx = (t * 7 + 3) % 16;
		uint2 p = uint2(idx % 4, idx / 4);
		wi.write(int4(int(t) + 1, -int(p.x) - 2, int(p.y) * 3 + 5, int(t * 2 + 7)), p);
		wh.write(half4(half(t) + 0.5h, -half(p.y) - 1.0h, half(p.x) * 2.0h + 0.25h, 1.0h), uint2(3 - p.x, p.y));
	}
	uint2 q = uint2(t % 4, t / 4);
	rwu.write(rwu.read(q) + uint4(1u, 2u, 3u, 4u), q);
	rws.write(short4(int4(rws.read(q)) + int4(int(q.x) + 1, -int(q.y) - 1, 3, -5)), q);
	dims[t] = uint2(wi.get_width() + rwu.get_height(), wh.get_height() + rws.get_width());
}
