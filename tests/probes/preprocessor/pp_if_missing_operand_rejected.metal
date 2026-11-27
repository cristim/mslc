// EXPECT: error expected a value in the #if expression
// A dangling operator.
#if 1 +
#endif
kernel void pp_if_missing_operand_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
