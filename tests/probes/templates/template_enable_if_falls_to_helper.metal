// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %v3float
// DISASM-MATCH: OpFunctionCall %float
// Refs #341: the template is for floating point only (the MetalPetal shape). An int argument drops
// it, so the float3 helper takes the call (the int is broadcast); without the enable_if it would
// win as an exact match with T = int and compute 3 / 2 = 1. A float vector keeps the template
// (is_floating_point holds for a vector), a float picks it over the helper's conversion.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_floating_point<T>::value>::type>
T halved(T x) { return x * T(0.5f); }
float3 halved(float3 x) { return float3(-1.0f); }
kernel void k(device float* out [[buffer(0)]])
{
	float3 fromInt = halved(3);
	float2 vec = halved(float2(5.0f));
	out[0] = halved(4.0f);
	out[1] = fromInt.x;
	out[2] = vec.y;
}
