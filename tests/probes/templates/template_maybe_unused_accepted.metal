// EXPECT: valid
// DISASM: OpFunctionCall %float
#include <metal_stdlib>
using namespace metal;

template <typename T>
[[maybe_unused]] inline __attribute__((always_inline)) T f(T x) {
	return x * T(2);
}

struct S {
	template <typename T>
	[[maybe_unused]] static T g(T x) {
		return x + T(1);
	}
};

kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
	out[i] = S::g(f(float(i)));
}
