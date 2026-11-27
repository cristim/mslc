// EXPECT: error _Pragma takes a parenthesised string literal
// The opening parenthesis is required.
kernel void pp_pragma_operator_without_an_open_paren_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
  _Pragma x "s" )
  out[i] = i;
}
