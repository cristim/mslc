// RUN: grid=16 local=16 cases=2
// A read_write 2D texture the kernel only writes: its own seed never reaches the output, so it is unused
// (the texels it does not write keep their seed on both runs and do not count).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read_write> w [[texture(0)]], device float *out [[buffer(0)]],
	uint t [[thread_position_in_grid]]) {
	if (t < 8)
		w.write(float4(float(t) + 1.0f), uint2(t % 4, t / 4));
	out[t] = float(t) + 1.0f;
}
