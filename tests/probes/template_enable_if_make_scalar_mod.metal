// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %v2float
// DISASM-MATCH: OpFunctionCall %half
// Refs #341: MetalPetal's mod: the default asks whether the element type of T is floating point
// (make_scalar), which holds for a scalar, a vector and a half. One instance per T.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename _E = typename enable_if<is_floating_point<typename make_scalar<T>::type>::value>::type>
T modulo(T x, T y) { return x - y * floor(x / y); }
kernel void k(device float* out [[buffer(0)]])
{
	float2 v = modulo(float2(5.5f, 7.0f), float2(2.0f, 4.0f));
	out[0] = modulo(5.5f, 2.0f);
	out[1] = v.y;
	out[2] = float(modulo(half(9.0h), half(4.0h)));
}
