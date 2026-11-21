// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %[_0-9a-zA-Z]+ 1 Offset 32
// DISASM-MATCH: ArrayStride 48
// Refs #341: an instance in a buffer has the layout of the struct it expands to (float3 is 16 bytes).
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct P { T a[N]; float f; };
kernel void k(device float* out [[buffer(0)]], device P<float3, 2>* in0 [[buffer(1)]], uint i [[thread_position_in_grid]])
{
	out[i] = in0[i].a[1].y + in0[i].f + float(sizeof(P<float3, 2>));
}
