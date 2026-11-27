// EXPECT: valid
// DISASM: OpConstant %int 3737
// DISASM-NOT: OpConstant %int 3838
// Apple's compiler defines __METAL__ and __METAL_VERSION__ (400 for the toolchain this was measured against).
kernel void pp_predefined_metal_macros(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if defined(__METAL__) && __METAL__ == 1 && __METAL_VERSION__ == 400 && __cplusplus >= 201703L
    out[i] = 3737;
#else
    out[i] = 3838;
#endif
 }
