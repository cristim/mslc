// EXPECT: valid
// Refs #341: slice 5d: an instance of an instance (Box<Box<T>>) and an instance with a template constant (Vn<float, N>), both named only inside function templates.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; T get() const { return v; } };
template <typename T, int N> struct Vn {
	T v[N];
	T sum() const { T s = T(0); for (int j = 0; j < N; ++j) s += v[j]; return s; }
};
template <typename T> float f(T x) { Box<Box<T>> b; b.v.v = x; return float(b.get().get()); }
template <int N> float g() { Vn<float, N> a; for (int j = 0; j < N; ++j) a.v[j] = float(j + 1); return a.sum(); }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	out[i] = f(7.0f) + f(2) + g<3>() + g<4>();
}
