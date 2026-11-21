// EXPECT: valid
// DISASM: OpTypeArray %float %uint_32
// DISASM: OpTypeArray %float %uint_6
//
// A template parameter is bound in each instance, so sizeof(T) is the size of what the call
// names: 16 * 2 for float3 and 2 * 3 for half. Apple's compiler agrees.
#include <metal_stdlib>
using namespace metal;
template <typename T, int N>
float fill()
{
    float a[sizeof(T) * N];
    a[0] = 1.0f;
    return a[0];
}
kernel void sizeof_template_parameter(device float *out [[buffer(0)]])
{
    out[0] = fill<float3, 2>() + fill<half, 3>();
}
