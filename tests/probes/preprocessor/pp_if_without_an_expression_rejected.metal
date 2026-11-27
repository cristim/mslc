// EXPECT: error #if with no expression
// #if needs an expression.
#if
#endif
kernel void pp_if_without_an_expression_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
