// EXPECT: valid
// DISASM: OpFunctionCall %float
// Refs #341: device T* and constant T* deduce T from a whole buffer argument, and device T* from a const
// device buffer deduces T = const float.
#include <metal_stdlib>
using namespace metal;
template <typename T> T readDevice(device T* p, uint i) { return p[i]; }
template <typename T> T readConstDevice(device const T* p, uint i) { return p[i]; }
template <typename T> T readConstant(constant T* p, uint i) { return p[i]; }
kernel void k(device float* out [[buffer(0)]], device const float* cin [[buffer(1)]],
              constant float* cbuf [[buffer(2)]], uint i [[thread_position_in_grid]])
{
    out[i] = readDevice(out, i) + readDevice(cin, i) + readConstDevice(cin, i) + readConstant(cbuf, i);
}
