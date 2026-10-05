// EXPECT: valid
//
// A trailing comma and an empty enum are legal.
#include <metal_stdlib>
using namespace metal;
enum Empty {};
enum Trail { X, Y, };
kernel void enum_trailing_comma_and_empty(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = Y; }
