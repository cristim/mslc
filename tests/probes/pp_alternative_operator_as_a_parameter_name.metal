// EXPECT: valid
// DISASM: OpConstant %int 6161
// Apple accepts an operator spelling as a parameter name.
#define PICK(and, or) or
kernel void pp_alternative_operator_as_a_parameter_name(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = PICK(1, 6161); }
