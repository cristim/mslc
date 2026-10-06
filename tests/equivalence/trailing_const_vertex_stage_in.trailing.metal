#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 position [[position]]; };
vertex Out f(In const in [[stage_in]]) { Out o; o.position = in.p; return o; }
