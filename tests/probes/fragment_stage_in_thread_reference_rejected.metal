// EXPECT: error is a reference, which Apple's compiler does not allow
//
// Apple rejects a reference as a fragment stage_in too.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; float4 c; };
fragment float4 fragment_stage_in_thread_reference_rejected(thread In& in [[stage_in]]) { return in.c; }
