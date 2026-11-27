// EXPECT: valid
// DISASM: OpConstant %int 3939
// DISASM-NOT: OpConstant %int 4040
// A comma inside parentheses yields its right side.
kernel void pp_if_comma_inside_parentheses(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ 
#if (0, 1) && !(1, 0)
    out[i] = 3939;
#else
    out[i] = 4040;
#endif
 }
