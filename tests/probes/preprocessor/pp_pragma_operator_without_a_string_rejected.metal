// EXPECT: error _Pragma takes a parenthesised string literal
// _Pragma takes a parenthesised string literal.
kernel void pp_pragma_operator_without_a_string_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  _Pragma(unroll)
  out[i] = i;
}
