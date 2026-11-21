// EXPECT: error _Pragma
// _Pragma is an operator, so it cannot be a macro name.
#define _Pragma 1
kernel void pp_define_pragma_operator_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
