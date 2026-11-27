// EXPECT: valid
// Refs #341: slice 5d: a member function template of a class template instance names another class template's instance, Other<U>, with U its own parameter.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Other { T w; T twice() const { return w + w; } };
template <typename T> struct Outer {
	T base;
	template <typename U> T mix(U x) const { Other<U> o; o.w = x; return base + T(o.twice()); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Outer<float> a{ 1.0f };
	Outer<int> c{ 2 };
	out[i] = a.mix(2.5f) + float(c.mix(3)) + a.mix(4);
}
