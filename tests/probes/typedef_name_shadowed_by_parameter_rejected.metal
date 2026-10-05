// EXPECT: error a variable named "index_t" would hide the typedef
//
// Apple allows a parameter to hide a typedef; mslc reports it, because the name would mean two things in one function.
#include <metal_stdlib>
using namespace metal;
typedef uint index_t;
kernel void typedef_name_shadowed_by_parameter_rejected(device uint* out [[buffer(0)]], uint index_t [[thread_position_in_grid]])
{ out[0] = index_t; }
