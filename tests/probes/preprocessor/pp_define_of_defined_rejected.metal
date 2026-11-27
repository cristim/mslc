// EXPECT: error cannot #define "defined"
// defined is an operator.
#define defined 1
kernel void pp_define_of_defined_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
