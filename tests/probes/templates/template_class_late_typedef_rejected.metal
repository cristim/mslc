// EXPECT: error is declared after the class template that uses it
// Refs #341: Apple rejects a typedef declared after the template; reading it at the use would also pick another type when an outer one exists.
#include <metal_stdlib>
using namespace metal;
template <typename T> struct P { T v; Late w; };
typedef float Late;
kernel void k(device float* out [[buffer(0)]]) { P<float> p; p.v = 1; out[0] = p.v; }
