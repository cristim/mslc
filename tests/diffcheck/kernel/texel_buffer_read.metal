// RUN: grid=64 local=16 cases=3
// A texture_buffer of each element type read at a thread-dependent texel (a different multiplier per buffer,
// modulo the width of buffer 0) and queried with get_width. Each buffer has its own texel count (256 plus 4 per
// texture index plus the case), so a folded or a wrong-buffer get_width shows, as does reading the wrong buffer,
// texel or component. Grid 64, local 16: the threads visit 64 distinct texels.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read> f [[texture(0)]], texture_buffer<half, access::read> h [[texture(1)]],
	texture_buffer<int, access::read> i [[texture(2)]], texture_buffer<uint, access::read> u [[texture(3)]],
	texture_buffer<short, access::read> s [[texture(4)]], texture_buffer<ushort, access::read> us [[texture(5)]],
	device float4 *of [[buffer(0)]], device half4 *oh [[buffer(1)]], device int4 *oi [[buffer(2)]],
	device uint4 *ou [[buffer(3)]], device short4 *os [[buffer(4)]], device ushort4 *ous [[buffer(5)]],
	device uint2 *width [[buffer(6)]], uint t [[thread_position_in_grid]]) {
	uint n = f.get_width();
	of[t] = f.read((t * 5 + 1) % n);
	oh[t] = h.read((t * 7 + 2) % n);
	oi[t] = i.read((t * 3 + 3) % n);
	ou[t] = u.read((t * 11 + 4) % n);
	os[t] = s.read(ushort((t * 13 + 5) % n));
	ous[t] = us.read((t * 17 + 6) % n);
	width[t] = uint2(n - 256, us.get_width() + h.get_width() - n);
}
