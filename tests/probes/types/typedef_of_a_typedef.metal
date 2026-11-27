// EXPECT: valid
//
// A typedef of a typedef resolves through both.
#include <metal_stdlib>
using namespace metal;
typedef uint index_t;
typedef index_t slot_t;
kernel void typedef_of_a_typedef(device slot_t* out [[buffer(0)]], slot_t i [[thread_position_in_grid]])
{ out[i] = i; }
