// RUN: grid=64 local=16 cases=2
// A read_write texel buffer the kernel only writes: its own seed never reaches the output, so it is unused
// (the texels it does not write keep their seed on both runs and do not count).
#include <metal_stdlib>
using namespace metal;
kernel void k(texture_buffer<float, access::read_write> w [[texture(0)]], device float *out [[buffer(0)]],
	uint t [[thread_position_in_grid]]) {
	w.write(float4(t), t);
	out[t] = float(t) + 1.0f;
}
