// EXPECT: error floating constant in the #if expression
// An #if is integer arithmetic.
#if 1.5
#endif
kernel void pp_if_float_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
