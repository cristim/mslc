// EXPECT: error cannot #define "__LINE__": it is a builtin
// __LINE__ is computed, not stored.
#define __LINE__ 5
kernel void pp_define_of_a_builtin_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
