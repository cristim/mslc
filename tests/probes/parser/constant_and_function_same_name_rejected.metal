// EXPECT: error redefinition of "kValue"
//
// Apple: "redefinition of 'kValue' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
constant int kValue = 1;
kernel void kValue(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
