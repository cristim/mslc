// EXPECT: error is a reference, which Apple's compiler does not allow
//
// Apple: type 'const constant In &' is not valid for attribute 'stage_in'.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; float4 c; };
fragment float4 fragment_stage_in_constant_reference_rejected(constant In& in [[stage_in]]) { return in.c; }
