// EXPECT: error C++ operator 'or' used as a macro name
// Apple's compiler refuses the spellings of C++ operators as macro names.
#undef or
kernel void pp_alternative_operator_as_a_macro_name_rejected_by_undef(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
