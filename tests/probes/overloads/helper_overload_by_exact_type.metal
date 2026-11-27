// EXPECT: valid
// DISASM: OpFunctionCall %float
// DISASM: OpFunctionCall %v3float
// DISASM: OpFunctionCall %int
// DISASM-NO-MATCH: OpFunctionCall %float %[A-Za-z_0-9]+ %v3float
// One name, three parameter types of one arity: each argument is an exact match for one of
// them, which is the overload Apple calls.
#include <metal_stdlib>
using namespace metal;
float f(float x) { return x; }
float3 f(float3 x) { return x * 2.0f; }
int f(int x) { return x + 1; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	const float a = 1.0f;
	const float3 b = float3(1.0f, 2.0f, 3.0f);
	out[i] = f(a) + f(b).z + float(f(7));
}
