// EXPECT: error has no fields
//
// A stage_in struct with nothing to read.
#include <metal_stdlib>
using namespace metal;
struct In { };
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_empty_struct_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
