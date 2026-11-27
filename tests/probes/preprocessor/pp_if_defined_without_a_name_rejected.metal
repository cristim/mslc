// EXPECT: error operator 'defined' requires an identifier
// defined needs a name.
#if defined
#endif
kernel void pp_if_defined_without_a_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
