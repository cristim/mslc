// EXPECT: error redefined differently
// 1+2 and 1 + 2 are different bodies in C, though the tokens match.
#define SUM 1+2
#define SUM 1 + 2
kernel void pp_redefinition_differing_in_whitespace_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
