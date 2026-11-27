// EXPECT: error typedef redefinition with different types
//
// const float and float are different types to a typedef. Apple: "typedef redefinition with different types".
#include <metal_stdlib>
using namespace metal;
typedef const float kC;
typedef float kC;
kernel void typedef_const_conflicting_redefinition_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
