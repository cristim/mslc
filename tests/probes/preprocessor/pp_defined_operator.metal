// EXPECT: valid
// DISASM: OpConstant %int 1212
// DISASM-NOT: OpConstant %int 1313
// defined X and defined(X), alone and combined.
#define PRESENT
kernel void pp_defined_operator(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if defined(PRESENT) && defined PRESENT && !defined(ABSENT) && !defined ABSENT
    out[i] = 1212;
#else
    out[i] = 1313;
#endif
 }
