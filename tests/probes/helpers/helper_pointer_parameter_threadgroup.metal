// EXPECT: valid
// DISASM-NOT: OpFunctionParameter %_ptr_Workgroup
// A threadgroup pointer parameter takes a threadgroup local array. (This probe pinned the refusal until it was
// lowered.)
#include <metal_stdlib>
using namespace metal;
float f(threadgroup float* p) { return p[0]; }
kernel void k(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ threadgroup float t[1]; t[0] = 1.0f; out[i] = f(t); }
