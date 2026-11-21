// RUN: grid=64 local=16 cases=3
// Write-only texture_buffers of each element type: thread t stores four different components at texel
// 3 * t + 5 (modulo the width), so a wrong texel, component or buffer changes the buffer contents, and the
// 192 texels no thread writes must keep their seed on both sides.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::write> f [[texture(0)]], texture_buffer<half, access::write> h [[texture(1)]],
	texture_buffer<int, access::write> i [[texture(2)]], texture_buffer<uint, access::write> u [[texture(3)]],
	texture_buffer<short, access::write> s [[texture(4)]], texture_buffer<ushort, access::write> us [[texture(5)]],
	uint t [[thread_position_in_grid]]) {
	uint n = f.get_width();
	uint j = (t * 3 + 5) % n;
	f.write(float4(t, t + 0.5f, -float(t), 1.0f), j);
	h.write(half4(half(t), half(2 * t), half(-1.5h), half(t) + 0.25h), j);
	i.write(int4(t, -int(t), 2 * int(t) + 7, int(n)), j);
	u.write(uint4(t, 3 * t, 4000000000u - t, n), j);
	s.write(short4(short(t), short(-int(t)), short(300), short(2 * t)), ushort(j));
	us.write(ushort4(ushort(t), ushort(65000 - t), ushort(7), ushort(n)), j);
}
