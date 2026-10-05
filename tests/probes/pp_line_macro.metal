// EXPECT: valid
// DISASM: OpConstant %int 8001
// __LINE__ is the line it is written on.
#line 8000
kernel void pp_line_macro(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = __LINE__; }
