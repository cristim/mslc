// EXPECT: error pasting formed '##'
// a ## ## b pastes a with ##, which is no token.
#define BAD a ## ## b
kernel void pp_paste_of_two_paste_operators_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
