// EXPECT: valid
// DISASM: OpConstant %uint 9191
// ## joins two tokens into one identifier.
#define JOIN(a, b) a##b
kernel void pp_token_paste(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ uint JOIN(res, ult) = 9191u; out[i] = result; }
