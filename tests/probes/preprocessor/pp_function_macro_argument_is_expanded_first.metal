// EXPECT: valid
// DISASM: OpConstant %int 6161
// An argument is macro-expanded before substitution.
#define VALUE 6161
#define IDENTITY(x) x
kernel void pp_function_macro_argument_is_expanded_first(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = IDENTITY(VALUE); }
