// EXPECT: error '#' is not followed by a macro parameter
// In a function-like macro # must name a parameter.
#define BAD(a) #b
kernel void pp_hash_without_a_parameter_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
