// EXPECT: error has [[point_size]], which a vertex function's [[stage_in]] struct cannot carry
#include <metal_stdlib>
using namespace metal;
struct In { float s [[point_size]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_point_size_vertex_input_rejected(In in [[stage_in]]) { Out o; o.p = float4(0.0); return o; }
