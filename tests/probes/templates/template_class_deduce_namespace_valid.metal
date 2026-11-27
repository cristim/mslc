// EXPECT: valid
// RUN: grid=4 local=4
// Refs #341: slice 5e: a class template and a function template in a namespace, and a function template outside naming N::Q<T>.
#include <metal_stdlib>
using namespace metal;
namespace N {
template <typename T> struct Q { float v; };
template <typename T> T z(Q<T> q) { return T(q.v); }
}
template <typename T> T outer(N::Q<T> q) { return T(q.v) + T(1); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	N::Q<int> a; a.v = 2.5f + float(i);
	N::Q<float> b; b.v = 2.5f + float(i);
	out[4 * i] = float(N::z(a));
	out[4 * i + 1] = N::z(b);
	out[4 * i + 2] = float(outer(a));
	out[4 * i + 3] = outer(b);
}
