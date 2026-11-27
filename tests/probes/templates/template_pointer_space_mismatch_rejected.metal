// EXPECT: error no matching function for call to "f"
// Refs #341: a constant buffer does not match device T*, and a device buffer does not match constant T*.
#include <metal_stdlib>
using namespace metal;
template <typename T> float f(device T* p) { return p[0]; }
kernel void k(device float* out [[buffer(0)]], constant float* cin [[buffer(1)]])
{
    out[0] = f(cin);
}
