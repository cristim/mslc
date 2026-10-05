// EXPECT: valid
// DISASM: OpConstant %int 4747
// DISASM-NOT: OpConstant %int 4646
// #undef removes a macro, so a later #ifdef is false.
#define GONE
#undef GONE
kernel void pp_undef(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#ifdef GONE
    out[i] = 4646;
#else
    out[i] = 4747;
#endif
 }
