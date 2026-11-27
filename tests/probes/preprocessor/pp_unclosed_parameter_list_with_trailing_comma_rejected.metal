// EXPECT: error expected a macro parameter name after ','
// A trailing comma in the parameter list.
#define BAD(a,) a
kernel void pp_unclosed_parameter_list_with_trailing_comma_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
