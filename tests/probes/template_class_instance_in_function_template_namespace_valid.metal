// EXPECT: valid
// Refs #341: slice 5d: the class template and the function template are in a namespace, and a caller outside names the function by its path.
#include <metal_stdlib>
using namespace metal;
namespace N {
	template <typename T> struct Box { T v; T get() const { return v; } };
	template <typename T> float f(T x) { Box<T> b; b.v = x; return float(b.get()); }
}
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	out[i] = N::f(1.5f) + N::f(4) + float(i);
}
