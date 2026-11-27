// EXPECT: error cannot store through "p"
// Refs #341: device T* deduced from a device const float* buffer binds T = const float, so writing through p is refused.
#include <metal_stdlib>
using namespace metal;
template <typename T> void writeAt(device T* p, uint i, float v) { p[i] = v; }
kernel void k(device float* out [[buffer(0)]], device const float* cin [[buffer(1)]],
              uint i [[thread_position_in_grid]])
{
    writeAt(cin, i, 1.0f);
    out[i] = 0.0f;
}
