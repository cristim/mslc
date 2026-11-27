// EXPECT: valid
// DISASM: OpConstant %int 7001
// #line sets the number of the next line, and may rename the file.
#line 7000 "renamed.metal"
kernel void pp_line_directive_renumbers(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = __LINE__; }
