// EXPECT: error not lowered yet
//
// A helper is not a stage, so a discard in one has no fragment to drop, and a
// helper that returns a value cannot drop the fragment that called it without
// changing the ABI mslc has fixed. Rejected rather than silently dropped.
// Apple accepts this same helper called from a fragment function.
#include <metal_stdlib>
using namespace metal;

float maybe_drop(float x)
{
    if (x > 0.5f) {
        discard_fragment();
    }
    return x;
}

fragment float4 frag(texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])
{
    return float4(maybe_drop(0.75f));
}