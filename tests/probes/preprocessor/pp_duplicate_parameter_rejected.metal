// EXPECT: error duplicate macro parameter "a"
// Parameter names are unique.
#define BAD(a, a) a
kernel void pp_duplicate_parameter_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
