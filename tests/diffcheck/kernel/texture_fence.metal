// RUN: grid=16 local=16 cases=3
// Write to read_write texture and texture_buffer, flush with fence(), then read back on the same thread.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read_write> a [[texture(0)]],
	texture_buffer<float, access::read_write> b [[texture(1)]],
	device float4* o [[buffer(0)]], uint t [[thread_position_in_grid]]) {
	uint2 p = uint2(t % 4, t / 4);
	float4 v = a.read(p) * 2.0f + float4(1.0f, 2.0f, 3.0f, 4.0f);
	a.write(v, p);
	a.fence();
	float4 reread = a.read(p);
	float4 initB = b.read(t);
	b.write(reread + initB * 0.5f, t);
	b.fence();
	o[t] = reread + b.read(t);
}
