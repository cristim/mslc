// EXPECT: valid
// DISASM: OpCapability Shader
//
// <metal_matrix> declares the matrix types, which mslc has built in, as
// test/lighting includes it.
#include <metal_matrix>
kernel void metal_matrix_include_accepted(device float4 *out [[buffer(0)]], constant float4x4 *m [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = m[i] * float4(1.0);
}
