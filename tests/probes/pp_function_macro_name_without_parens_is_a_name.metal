// EXPECT: valid
// DISASM: OpConstant %uint 8181
// DISASM-NOT: OpIMul
// A function-like macro is only invoked when '(' follows its name.
#define scale(x) ((x) * 3u)
kernel void pp_function_macro_name_without_parens_is_a_name(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ uint scale = 8181u; out[i] = scale; }
