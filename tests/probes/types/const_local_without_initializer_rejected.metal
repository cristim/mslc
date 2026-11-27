// EXPECT: error the const variable "v" needs an initialiser
//
// Apple: "default initialization of an object of const type 'const float'".
#include <metal_stdlib>
using namespace metal;
kernel void const_local_without_initializer_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ const float v; out[i] = i; }
