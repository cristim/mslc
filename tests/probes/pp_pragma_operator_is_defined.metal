// EXPECT: valid
// DISASM: "pp_pragma_operator_is_defined"
// _Pragma is an operator Apple's compiler has, so defined() answers true as for __has_include.
#if !defined(_Pragma)
#error _Pragma is not defined
#endif
#ifndef _Pragma
#error _Pragma is not defined
#endif
kernel void pp_pragma_operator_is_defined(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
