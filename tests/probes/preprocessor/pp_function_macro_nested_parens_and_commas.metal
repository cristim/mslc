// EXPECT: valid
// DISASM: OpConstant %int 5151
// A comma inside parentheses does not split the arguments.
#define SECOND(a, b) b
kernel void pp_function_macro_nested_parens_and_commas(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = SECOND((1, 2), 5151); }
