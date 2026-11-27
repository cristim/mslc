// EXPECT: error is a reference, which Apple's compiler does not allow
//
// Apple: type 'In &' is not valid for attribute 'stage_in'. The struct is taken by value.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[attribute(0)]]; };
struct Out { float4 p [[position]]; };
vertex Out vertex_stage_in_thread_reference_rejected(thread In& in [[stage_in]]) { Out o; o.p = in.p; return o; }
