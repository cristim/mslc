// EXPECT: error has [[attribute(n)]], which mslc does not lower on a struct crossing
//
// Apple accepts [[attribute(n)]] on a fragment function's stage_in struct, where
// it has no meaning; mslc reports it rather than guess the intent.
#include <metal_stdlib>
using namespace metal;
struct In { float4 p [[position]]; float4 c [[attribute(0)]]; };
fragment float4 fragment_stage_in_attribute_rejected(In in [[stage_in]]) { return in.c; }
