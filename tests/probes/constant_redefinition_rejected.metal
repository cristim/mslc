// EXPECT: error redefinition of "kValue"
//
// Apple: "redefinition of 'kValue'". Two file-scope constants of one name used to compile, the second shadowing the first.
#include <metal_stdlib>
using namespace metal;
constant int kValue = 1;
constant int kValue = 2;
kernel void constant_redefinition_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
