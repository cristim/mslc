// EXPECT: valid
// DISASM: OpConstant %int 7878
// DISASM-NOT: OpConstant %int 7979
// The second include still defines what only <metal_stdlib> does.
#include <metal_matrix>
#include <metal_stdlib>
kernel void pp_metal_matrix_then_metal_stdlib(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if defined(__BITS_METAL_TEXTURE) && defined(__METAL_MATRIX_H)
    out[i] = 7878;
#else
    out[i] = 7979;
#endif
 }
