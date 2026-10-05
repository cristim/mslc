// EXPECT: valid
// DISASM: OpConstant %int 7272
// DISASM-NOT: OpConstant %int 7373
// metal_types is imported without an #include, so its macros exist from the first line (checked against xcrun metal).
kernel void pp_implicit_metal_types_macros(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if defined(METAL_FUNC) && defined(__HAVE_FMA__) && INT_MAX == 2147483647 && defined(M_PI_F) && defined(FLT_MAX) && defined(INFINITY)
    out[i] = 7272;
#else
    out[i] = 7373;
#endif
 }
