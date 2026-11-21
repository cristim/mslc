// EXPECT: valid
// Refs #341: slice 5d: a parameter Box<T> of a function template with explicit arguments, which fix T before the signature is read.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
template <typename T> T unbox(Box<T> b) { return b.get() + T(1); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> b{ 4.0f };
	out[i] = unbox<float>(b) + float(i);
}
