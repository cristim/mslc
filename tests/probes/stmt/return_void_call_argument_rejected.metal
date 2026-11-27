// EXPECT: error a call to "w" returns void and has no value
// Only the returned call may be void, not one in its arguments.
#include <metal_stdlib>
using namespace metal;
inline void w() {}
inline float g(float x) { return x; }
inline void v() { return (void)g(w()); }
kernel void k(device float* out [[buffer(0)]]) { v(); out[0] = 1; }
