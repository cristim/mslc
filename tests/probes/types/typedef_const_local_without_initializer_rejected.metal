// EXPECT: error the const variable "v" needs an initialiser
//
// The qualifier comes from the typedef. Apple: "default initialization of an object of const type 'T' (aka 'const float')".
#include <metal_stdlib>
using namespace metal;
typedef const float T;
kernel void typedef_const_local_without_initializer_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ T v; out[i] = i; }
