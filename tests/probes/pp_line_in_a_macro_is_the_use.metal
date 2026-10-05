// EXPECT: valid
// DISASM: OpConstant %int 9003
// DISASM-NOT: OpConstant %int 9000
// A __LINE__ in a macro body is the line of the macro's use, not of its definition.
#line 9000
#define HERE __LINE__

kernel void pp_line_in_a_macro_is_the_use(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = HERE; }
