// EXPECT: valid
// DISASM: OpConstant %int 7676
// DISASM-NOT: OpConstant %int 7777
// <metal_matrix> defines __METAL_MATRIX_H but not the rest of what <metal_stdlib> does.
#include <metal_matrix>
kernel void pp_metal_matrix_adds_fewer_macros(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if defined(__METAL_MATRIX_H) && !defined(__BITS_METAL_TEXTURE) && !defined(assert)
    out[i] = 7676;
#else
    out[i] = 7777;
#endif
 }
