// EXPECT: error #line directive requires a simple digit sequence
// #line takes digits.
#line "file.metal"
kernel void pp_line_directive_needs_a_number_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
