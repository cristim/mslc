// EXPECT: error attributes on function template "f" are not lowered yet
#include <metal_stdlib>
using namespace metal;

template <typename T>
T f(T x) __attribute__((always_inline)) [[maybe_unused]] {
	return x;
}

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = f(1.0f);
}
