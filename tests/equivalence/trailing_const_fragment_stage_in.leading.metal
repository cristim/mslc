#include <metal_stdlib>
using namespace metal;
struct In { float4 position [[position]]; float2 uv; };
fragment float4 f(const In in [[stage_in]]) { return in.position + float4(in.uv, 0.0, 0.0); }
