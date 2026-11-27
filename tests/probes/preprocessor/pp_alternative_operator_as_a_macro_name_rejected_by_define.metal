// EXPECT: error C++ operator 'and' used as a macro name
// Apple's compiler refuses the spellings of C++ operators as macro names.
#define and 1
kernel void pp_alternative_operator_as_a_macro_name_rejected_by_define(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
