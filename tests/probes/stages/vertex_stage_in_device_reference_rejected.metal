// EXPECT: error is a reference, which Apple's compiler does not allow
//
// Apple rejects a device reference as stage_in.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_stage_in_device_reference_rejected(device In& in [[stage_in]]) { Out o; o.p = in.p; return o; }
