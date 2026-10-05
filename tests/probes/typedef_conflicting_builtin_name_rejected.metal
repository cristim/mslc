// EXPECT: error typedef redefinition with different types
//
// A typedef cannot rebind a builtin type name or one of the simd typedefs to something else.
#include <metal_stdlib>
using namespace metal;
typedef int vector_float2;
kernel void typedef_conflicting_builtin_name_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
