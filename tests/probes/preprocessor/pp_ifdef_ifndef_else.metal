// EXPECT: valid
// DISASM: OpConstant %int 1515
// DISASM-NOT: OpConstant %int 1414
// DISASM-NOT: OpConstant %int 1616
// #ifdef, #ifndef and #else on a defined and an undefined name.
#define PRESENT
kernel void pp_ifdef_ifndef_else(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#ifdef ABSENT
    out[i] = 1414;
#else
    out[i] = 1515;
#endif
#ifndef PRESENT
    out[i] = 1616;
#endif
 }
