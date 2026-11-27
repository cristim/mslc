// EXPECT: error macro name missing in #undef
// #undef needs a name.
#undef
kernel void pp_undef_without_a_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
