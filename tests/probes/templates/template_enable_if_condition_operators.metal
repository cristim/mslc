// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %float
// Refs #341: the condition is an ordinary constant expression over the traits: || and && with a
// parenthesised comparison, sizeof of the deduced type, and a non-type parameter (N > 2) whose ">"
// is inside parentheses so it does not close the list. Each template answers for some of the types,
// the helper f(float3) for the rest, and the sum distinguishes every pick.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename E = typename enable_if<is_integral<T>::value || is_same<T, half>::value>::type>
float f(T x) { return 1.0f; }
template <typename T, typename E = typename enable_if<(sizeof(T) > 8) && is_floating_point<T>::value>::type>
float h(T x) { return 2.0f; }
template <typename T, int N = 3, typename E = typename enable_if<(N > 2) && is_unsigned<T>::value>::type>
float g(T x) { return 10.0f * N; }
float f(float3 x) { return 100.0f; }
float g(float3 x) { return 200.0f; }
float h(float3 x) { return 300.0f; }
kernel void k(device float* out [[buffer(0)]])
{
	out[0] = f(1) + f(half(1.0h));
	out[1] = h(float4(1)) + h(1.0f);
	out[2] = g(1u) + g(1.0f);
	out[3] = g<uint, 5>(1u);
}
