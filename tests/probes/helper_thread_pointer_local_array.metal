// EXPECT: valid
// DISASM-MATCH: OpTypePointer Function %_arr_v4float_uint_8
// DISASM-MATCH: OpTypePointer Function %_arr_float_uint_4
// DISASM-NOT: OpPtrAccessChain
//
// Apple accepts a helper "thread T *p" given a local array. mslc emits the helper once per
// array length, with the parameter a pointer to the whole array, so p[i] with a run-time
// index is an OpAccessChain and the helper writes the caller's storage. The two call sites
// have different lengths, so there are two helpers.
#include <metal_stdlib>
using namespace metal;
void fill(thread float4 *p, int n) { for (int i = 0; i < n; i++) { p[i] = float4(i); } *p = float4(9.0); }
float sum(const thread float *p, int n) { float s = 0.0; for (int i = 0; i < n; i++) { s += p[i]; } return s; }
float twice(thread float *q) { q[1] = 5.0; return sum(q, 2); }
kernel void helper_thread_pointer_local_array(device float *out [[buffer(0)]], uint t [[thread_position_in_grid]])
{
	float4 a[8];
	fill(a, 8);
	float b[4] = { 1.0, 2.0, 3.0, 4.0 };
	out[t] = a[3].x + a[0].y + sum(b, 4) + twice(b) + b[1];
}
