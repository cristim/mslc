// EXPECT: error unsupported attribute "attribute"
//
// Apple rejects [[attribute(n)]] on a vertex parameter: "invalid type 'float4'
// for input declaration in a vertex function". The attributes come in through a
// [[stage_in]] struct.
#include <metal_stdlib>
using namespace metal;
struct Out { float4 p [[position]]; };
vertex Out vertex_attribute_on_parameter_rejected(float4 a [[attribute(0)]]) { Out o; o.p = a; return o; }
