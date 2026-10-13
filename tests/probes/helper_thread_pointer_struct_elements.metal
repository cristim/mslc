// EXPECT: valid
// DISASM-NOT: OpPtrAccessChain
//
// A thread pointer to a struct reads and writes fields and swizzles through p[i], through
// "p->" (element zero) and through (*p), at constant and run-time indices.
#include <metal_stdlib>
using namespace metal;
struct S { float x; float3 v; };
float touch(thread S *p, int i) { p[i].x = 2.0; p[i].v.y = 3.0; p->x += 1.0; (*p).v.z = 4.0; return p[i].v.y + p[0].v.z; }
kernel void helper_thread_pointer_struct_elements(device float *out [[buffer(0)]], uint t [[thread_position_in_grid]])
{
	S a[3];
	a[1].x = 0.0; a[1].v = float3(0.0); a[0].x = 0.0; a[0].v = float3(0.0);
	out[t] = touch(a, int(t)) + a[0].x;
}
