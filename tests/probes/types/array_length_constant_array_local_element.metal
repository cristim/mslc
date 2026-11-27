// EXPECT: valid
// DISASM-NO-MATCH: OpTypeArray %float %uint_2[^0-9]
// DISASM-MATCH: OpTypeArray %float %uint_3[^0-9]
//
// A local constexpr integer array can be indexed in a constant expression too:
// the length of the second array is the element k[1], which is 3.
#include <metal_stdlib>
using namespace metal;
kernel void array_length_constant_array_local_element(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    constexpr uint k[] = {2, 3};
    float a[k[1]];
    a[0] = 1.0;
    a[1] = 2.0;
    a[2] = 3.0;
    out[i] = a[2];
}
