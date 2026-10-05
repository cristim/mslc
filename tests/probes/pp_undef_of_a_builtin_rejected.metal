// EXPECT: error cannot #undef "__COUNTER__"
// A builtin cannot be removed.
#undef __COUNTER__
kernel void pp_undef_of_a_builtin_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
