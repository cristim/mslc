// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: Box<const T> given Box<const float> deduces T = float (the const is spelled by the pattern, not part of T); Apple gives is_const 0 and is_same 1.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> float c(Box<const T> b)
{
	T x = b.v;
	x += T(1);
	return float(is_const<T>::value) * 2.0f + float(is_same<T, float>::value) * 10.0f + float(x);
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<const float> b = { 1.5f + float(i) };
	out[i] = c(b);
}
