// EXPECT: error character constants in the #if expression are not supported
// Named: the lexer has no character literals.
#if 'a' == 97
#endif
kernel void pp_if_character_constant_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
