// EXPECT: error _Pragma takes a parenthesised string literal
// The closing parenthesis is required.
kernel void pp_pragma_operator_without_a_close_paren_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  _Pragma("unroll" x)
  out[i] = i;
}
