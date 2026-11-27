// EXPECT: error the [[function_constant]] gate "gate" is a vector
// A gate is a scalar constant that is on or off; a vector is neither.
#include <metal_stdlib>
using namespace metal;
constant float2 gate = float2(1.0f, 0.0f);
struct Out { float4 color [[color(0)]]; uint mask [[sample_mask, function_constant(gate)]]; };
fragment Out function_constant_gated_sample_mask_vector_rejected() { return Out{ float4(0.0f), 1u }; }
