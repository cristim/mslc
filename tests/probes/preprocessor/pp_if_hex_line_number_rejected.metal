// EXPECT: error #line directive requires a simple digit sequence
// #line takes decimal digits only.
#line 0x10
kernel void pp_if_hex_line_number_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
