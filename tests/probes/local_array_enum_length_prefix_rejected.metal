// EXPECT: error brackets are not allowed here
//
// An enumerator is a constant array length, but Apple rejects "float[Count] v;" ("brackets are not allowed here"), so the length goes after the name.
#include <metal_stdlib>
using namespace metal;
enum { Count = 3 };
kernel void local_array_enum_length_prefix_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ float[Count] v; out[i] = i; }
