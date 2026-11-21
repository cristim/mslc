// EXPECT: valid
// An alias keeps what the texture is: a depth texture is sampled as one (a scalar result) and a
// write-only texture is written through its alias.
// DISASM: OpTypeImage %float 2D 1 0 0 1 Unknown
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
kernel void k(depth2d<float> d [[texture(0)]], texture2d<float, access::write> o [[texture(1)]],
	sampler s [[sampler(0)]], uint2 g [[thread_position_in_grid]])
{
	depth2d<float> b = d;
	texture2d<float, access::write> w = o;
	w.write(float4(b.sample(s, float2(0.5))), g);
}
