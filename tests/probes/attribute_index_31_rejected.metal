// EXPECT: error attribute "attribute" index 31 is out of bounds
//
// Apple: "'attribute' attribute parameter is out of bounds: must be between 0 and 30", even for a struct nothing uses.
#include <metal_stdlib>
using namespace metal;
struct In { float4 a [[attribute(31)]]; };
kernel void attribute_index_31_rejected(device uint* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
