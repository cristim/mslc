// EXPECT: valid
// DISASM: OpLoopMerge
// Refs #341: a non-type parameter is a constant inside the instance, an array length included.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N> struct Arr {
	T a[N];
	Arr(T x) { for (int i = 0; i < N; ++i) a[i] = x; }
	T sum() const { T s = T(0); for (int i = 0; i < N; ++i) s += a[i]; return s; }
};
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
	Arr<float, 3> a(2.0f);
	out[i] = a.sum();
}
