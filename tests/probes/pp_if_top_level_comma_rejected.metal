// EXPECT: error missing a binary operator before ","
// A comma outside parentheses is not an #if operator.
#if 1, 2
#endif
kernel void pp_if_top_level_comma_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
