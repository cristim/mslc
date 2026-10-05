// EXPECT: error cannot #define "float": it is a keyword
// #define float int would change what every later declaration means, so it is refused.
#define float int
kernel void pp_define_of_a_keyword_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
