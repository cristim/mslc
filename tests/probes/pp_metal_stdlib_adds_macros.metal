// EXPECT: valid
// DISASM: OpConstant %int 7474
// DISASM-NOT: OpConstant %int 7575
// <metal_stdlib> is built in, but the macros it defines are real: absent before the include, present after.
#ifdef __METAL_MATH
#define BEFORE 1
#else
#define BEFORE 0
#endif
#include <metal_stdlib>
kernel void pp_metal_stdlib_adds_macros(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if BEFORE == 0 && defined(__METAL_MATH) && defined(__METAL_MAYBE_FAST_MATH__)
    out[i] = 7474;
#else
    out[i] = 7575;
#endif
 }
