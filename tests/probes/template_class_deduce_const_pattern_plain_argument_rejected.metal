// EXPECT: error no matching function for call to "c"; candidates: ; templates: template
// Refs #341: slice 5e: Box<const T> does not match Box<float>; Apple rejects it.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct Box { T v; };
template <typename T> float c(Box<const T> b) { return float(b.v); }
kernel void k(device float* out [[buffer(0)]]) { Box<float> b; b.v = 1.5f; out[0] = c(b); }
