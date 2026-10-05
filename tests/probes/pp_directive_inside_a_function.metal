// EXPECT: valid
// DISASM: OpConstant %int 4444
// DISASM-NOT: OpConstant %uint 4545
// #define, #undef and #if work inside a function body.
kernel void pp_directive_inside_a_function(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#define LOCAL 4444
    uint a = LOCAL;
#undef LOCAL
#ifdef LOCAL
    a = 4545u;
#endif
    out[i] = a;
 }
