// EXPECT: valid
// DISASM: OpConstant %int 6666
// DISASM-NOT: OpConstant %int 6767
// __has_include answers from the same search an #include does.
kernel void pp_has_include(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if __has_include("include/pp_defs.h") && !__has_include("include/not_there.h") && __has_include(<metal_stdlib>)
    out[i] = 6666;
#else
    out[i] = 6767;
#endif
 }
