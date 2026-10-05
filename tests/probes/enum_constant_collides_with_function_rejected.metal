// EXPECT: error redefinition of "helper"
//
// Apple: "redefinition of 'helper' as different kind of symbol".
#include <metal_stdlib>
using namespace metal;
enum One { helper };
kernel void helper(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
