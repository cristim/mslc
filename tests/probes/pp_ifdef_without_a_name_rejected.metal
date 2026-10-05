// EXPECT: error #ifdef expects a macro name
// #ifdef needs a name.
#ifdef
#endif
kernel void pp_ifdef_without_a_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
