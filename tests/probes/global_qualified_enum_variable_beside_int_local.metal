// EXPECT: valid
// Color x = ::gc: the file-scope enum constant, not the int local gc.
#include <metal_stdlib>
using namespace metal;
enum Color { red, green }; constant Color gc = green; kernel void kern(device int* out [[buffer(0)]]) { int gc = 7; Color x = ::gc; out[0] = int(x) + gc; }
