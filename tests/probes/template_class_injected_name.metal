// EXPECT: valid
// Refs #341: inside the template, its own name (alone or with its arguments) is the instance: a constructor is declared by it,
// and V, V<T> and V(...) name the same struct.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct V {
	T x; T y;
	V(T a, T b) : x(a), y(b) {}
	V<T> add(V<T> o) const { return V<T>(x + o.x, y + o.y); }
	V swap() const { V r(y, x); return r; }
	static V<T> make(T a) { return V(a, a); }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	V<float> a(1.0f, 2.0f);
	V<float> b = a.add(a.swap());
	V<float> c = V<float>::make(4.0f);
	out[i] = b.x + b.y + c.x;
}
