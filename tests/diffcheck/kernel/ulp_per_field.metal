// RUN: grid=64 local=16 cases=3
// One struct buffer whose fields have different ULP bounds (`ulp_bounds_by_root`), with no DIFFCHECK-ULP directive:
// `root` calls sqrt (2 ULP), `loose` calls exp (4 ULP), and `exact` is an exactly rounded multiply (0 ULP).
#include <metal_stdlib>
using namespace metal;
struct Out {
	float root;
	float loose;
	float exact;
};
kernel void k(device const float *in [[buffer(0)]], device Out *out [[buffer(1)]], uint i [[thread_position_in_grid]]) {
	out[i].root = sqrt(in[i]);
	out[i].loose = exp(in[i]);
	out[i].exact = in[i] * 3.0f;
}
