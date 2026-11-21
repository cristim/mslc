// EXPECT: valid
// Color x = ::gc is green (1), not the local red.
#include <metal_stdlib>
using namespace metal;
enum Color { red, green }; constant Color gc = green; kernel void kern(device int* out [[buffer(0)]]) { Color gc = red; Color x = ::gc; out[0] = int(x); }
