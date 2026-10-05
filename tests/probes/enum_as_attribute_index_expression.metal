// EXPECT: valid
// REFLECT: "metal_index": 3
//
// The attribute index is a constant expression, so Base + 1 is 3.
#include <metal_stdlib>
using namespace metal;
enum { Base = 2 };
kernel void enum_as_attribute_index_expression(device uint* out [[buffer(Base + 1)]], uint i [[thread_position_in_grid]])
{ out[i] = i; }
