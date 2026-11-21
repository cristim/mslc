// EXPECT: valid
// DISASM: OpFunctionCall %float
// Apple's ranking of a scalar argument over f(int)/f(float): an exact type wins, a bool or an
// int-sized integer promotes to int, and a scalar reaches a vector only after every scalar
// conversion (g(2) is g(float), not a splat into g(float3)).
#include <metal_stdlib>
using namespace metal;
float f(int x) { return 1.0f; }
float f(float x) { return 2.0f; }
float g(float3 x) { return 10.0f; }
float g(float x) { return 20.0f; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	const bool b = true;
	out[i] = f(b) + f(1) + f(1.0f) + g(2) + g(float3(1.0f));
}
