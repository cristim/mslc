// EXPECT: error which is not a depth2d_ms<float, access::sample>
// Apple distinguishes depth2d_ms<float> (access::read) from depth2d_ms<float, access::sample>.
#include <metal_stdlib>
using namespace metal;
float readSample(depth2d_ms<float, access::sample> t, uint2 p) { return t.read(p, 0); }
kernel void hit(depth2d_ms<float> a [[texture(0)]], device float* o [[buffer(0)]]) {
  o[0] = readSample(a, uint2(0));
}
