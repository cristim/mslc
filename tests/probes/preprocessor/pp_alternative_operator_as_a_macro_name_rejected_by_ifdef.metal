// EXPECT: error C++ operator 'not' used as a macro name
// Apple's compiler refuses the spellings of C++ operators as macro names.
#ifdef not
#endif
kernel void pp_alternative_operator_as_a_macro_name_rejected_by_ifdef(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
