// EXPECT: valid
// Refs #341: a member function template of a class template instance reads the instance's own T.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box {
	T v;
	template <typename U> T add(U x) const { return v + T(x); }
	template <typename U = T> U scale(U x) const { return U(v) * x; }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Box<float> b{1.5f};
	Box<int> c{2};
	out[i] = b.add(2) + float(c.add(2.5f)) + b.scale(2.0f) + float(c.scale(3));
}
