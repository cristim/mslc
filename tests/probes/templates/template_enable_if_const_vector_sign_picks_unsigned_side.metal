// EXPECT: valid
// RUN: grid=1 local=1
// DISASM: %int_2 = OpConstant %int 2
// Refs #341: Apple says is_signed and is_unsigned are both false for a const vector, so the template
// asking for the negation is the one f<const float4> and f<const int2> pick (1 for the signed
// scalar, 2 for the const vectors). Reading the element's sign picked the other template.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename = enable_if_t<is_signed_v<T>>> int f(T) { return 1; }
template <typename T, typename = enable_if_t<!is_signed_v<T>>, typename = void> int f(T) { return 2; }
template <typename T> int viaConst() { return is_signed_v<const T> ? 1 : 2; }
kernel void k(device int* out [[buffer(0)]])
{
	out[0] = f<const float4>(float4(1));
	out[1] = f<const int2>(int2(1));
	out[2] = f<const int>(1);
	out[3] = f<float4>(float4(1));
	out[4] = viaConst<uint3>();
}
