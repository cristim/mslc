// RUN: grid=64 local=16 cases=1
// DIFFCHECK-ULP: exact 0
// `exp` is not exactly rounded, so the file as a whole gets a 4 ULP bound, which would also hide
// a one ULP error in the exactly rounded `exact` buffer. The directive holds that buffer to 0.
#include <metal_stdlib>
using namespace metal;
kernel void k(device const float *in [[buffer(0)]], device float *loose [[buffer(1)]], device float *exact [[buffer(2)]],
	uint i [[thread_position_in_grid]]) {
	loose[i] = exp(in[i]);
	exact[i] = in[i] * 3.0f;
}
