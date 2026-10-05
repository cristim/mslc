// EXPECT: error pasting formed '##'
// The comment between them changes nothing.
#define BAD a ## /* c */ ## b
kernel void pp_paste_of_two_paste_operators_across_a_comment_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
