// EXPECT: error is [[color(n)]], which cannot also carry another attribute

#include <metal_stdlib>
using namespace metal;

// Apple accepts the pair; mslc refuses it, because an interpolation has nothing to apply to.
struct In {
  float4 c [[color(0), flat]];
  float4 p [[position]];
};

fragment float4 framebuffer_fetch_field_with_interpolation_rejected(In in [[stage_in]]) { return in.c; }
