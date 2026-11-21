// RUN: grid=64 local=16 cases=3
// A 4x4 float texture read texel by texel, coordinates (i mod 4, i / 4 mod 4) so the 64 threads
// visit every texel four times: swapped coordinates, a wrong texture or a wrong binding change
// the output. Grid and local size differ so a thread index mix-up shows too.
#include <metal_stdlib>
using namespace metal;
kernel void k(texture2d<float, access::read> t [[texture(0)]], device float4 *out [[buffer(0)]],
	uint i [[thread_position_in_grid]]) {
	out[i] = t.read(uint2(i % 4, i / 4 % 4));
}
