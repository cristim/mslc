// EXPECT: valid
// Refs #341: slice 5d: a member function template of a plain struct names an instance built from its parameter U.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
struct S {
	float base;
	template <typename U> float g(U x) const { Box<U> b; b.v = x; return base + float(b.get()); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	S s;
	s.base = 1.0f;
	out[i] = s.g(2.5f) + s.g(3);
}
