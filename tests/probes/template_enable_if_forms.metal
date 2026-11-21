// EXPECT: valid
// RUN: grid=1 local=1
// DISASM-MATCH: OpFunctionCall %float
// DISASM: OpFunctionParameter %float
// DISASM: OpFunctionParameter %half
// DISASM: OpFunctionParameter %uint
// Refs #341: the spellings of an enable_if default: enable_if_t, an unnamed parameter, class,
// a second argument (the default type), is_same_v, a closing ">>" of two lists, a negation, and an
// enable_if that is the return type. Each of the six returns a different value for the float
// call, and each is dropped for the int call, so only the non-template int overload answers it.
#include <metal_stdlib>
using namespace metal;
template <typename T, typename = enable_if_t<is_same_v<T, float>>> float a(T x) { return 10.0f; }
template <class T, class E = enable_if_t<is_floating_point<T>::value, int>> float b(T x) { return 20.0f; }
template <typename T, typename E = typename enable_if<!is_integral<T>::value, T>::type> float c(T x) { return 30.0f; }
template <typename T> typename enable_if<is_floating_point<T>::value, float>::type d(T x) { return 40.0f; }
template <typename T> enable_if_t<is_arithmetic<T>::value && !is_vector<T>::value, float> e(T x) { return 50.0f; }
float a(int x) { return 1.0f; }
float b(int x) { return 2.0f; }
float c(int x) { return 3.0f; }
float d(int x) { return 4.0f; }
float e(float3 x) { return 5.0f; }
kernel void k(device float* out [[buffer(0)]])
{
	out[0] = a(1.5f) + b(1.5f) + c(1.5f) + d(half(1.5h)) + e(2u);
	out[1] = a(1) + b(1) + c(1) + d(1) + e(float3(1));
}
