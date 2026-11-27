// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
// DISASM: OpVariable %_ptr_Function_uint Function
// An assignment to a builtin parameter in a branch: the copy is made at entry, so the read after the branch
// sees the stored value where the branch ran and the builtin's elsewhere.
#include <metal_stdlib>
using namespace metal;
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]], uint l [[thread_index_in_threadgroup]]) { uint j = i; if (l > 2) { l = 7; } i += 1; out[j] = float(l * 100 + i); }
