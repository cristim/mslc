// RUN: grid=16 local=16 cases=3
// Two write-only 2D float textures at indices 2 and 0: threads 0..10 write 11 of the 16 texels (coordinates
// ((7 * t + 3) % 16 % 4, (7 * t + 3) % 16 / 4), so five texels are never written and must keep their seed on both
// sides). Swapping the two texture bindings, swapping x and y of a write coordinate, or swapping two float4 components
// changes the texture contents.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::write> a [[texture(2)]], texture2d<float, access::write> b [[texture(0)]],
	device uint2 *dims [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	if (t < 11) {
		uint idx = (t * 7 + 3) % 16;
		uint2 p = uint2(idx % 4, idx / 4);
		a.write(float4(float(t) + 1.0f, float(p.x) * 2.0f + 0.5f, -float(p.y) - 1.0f, float(t * 2 + 3)), p);
		b.write(float4(float(p.y) + 10.0f, -float(t) - 0.25f, float(p.x) + 3.0f, 1.0f), uint2(3 - p.x, p.y));
	}
	dims[t] = uint2(a.get_width(), b.get_height());
}
