// EXPECT: valid
// DISASM: OpConstant %int 5858
// ZERO() is a call with no arguments, not one empty argument.
#define ZERO() 5858
kernel void pp_function_macro_with_no_parameters(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = ZERO(); }
