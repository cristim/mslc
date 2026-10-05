// EXPECT: error invalid numeric literal "2B"
// 2 ## B is one token in C, a number that is no literal. It must not become 2 and a name B.
#define B 7
#define JOIN(a, b) a##b
kernel void pp_paste_forming_a_malformed_number_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = JOIN(2, B); }
