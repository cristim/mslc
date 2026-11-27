// EXPECT: error typedef redefinition with different types
//
// Apple: "typedef redefinition with different types ('int' vs 'float')".
#include <metal_stdlib>
using namespace metal;
typedef float scalar_t;
typedef int scalar_t;
kernel void typedef_conflicting_redefinition_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
