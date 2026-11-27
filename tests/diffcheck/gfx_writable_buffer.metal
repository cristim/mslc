// RUN: vertex=vs fragment=fs instances=2
// DIFFCHECK-POSITION: pos
// A fragment function that writes a device buffer across multiple instances: invocation order would make it racy, so it is skipped.
#include <metal_stdlib>
using namespace metal;
struct VIn { float2 pos [[attribute(0)]]; };
vertex float4 vs(VIn in [[stage_in]]) { return float4(in.pos, 0.5, 1.0); }
fragment float4 fs(device float* log [[buffer(0)]]) { log[0] = 1.0; return float4(1.0); }
