// EXPECT: valid
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 Offset 64[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 MatrixStride 16[^0-9]
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 1 ColMajor
// DISASM-MATCH: OpMemberDecorate %_struct_[0-9]+ 2 Offset 112[^0-9]
//
// A float3x3 is three float3 columns, and a float3 is 16 bytes in Metal, so the
// matrix is 48 bytes with a 16-byte column stride rather than nine packed
// floats. The field after it therefore starts at 64 + 48 = 112. A stride of 12
// validates and reads the padding as matrix elements, so the numbers are pinned
// here; the values were checked by a lavapipe read-back against a host
// reference. This is the Uniforms struct of indium's lighting test.
struct Uniforms
{
    float4x4 modelViewProjectionMatrix;
    float3x3 normalMatrix;
    float tail;
};

kernel void matrix_struct_layout(device float3 *out [[buffer(0)]],
                                 constant Uniforms &uniforms [[buffer(1)]],
                                 device const float3 *in [[buffer(2)]],
                                 uint i [[thread_position_in_grid]])
{
    float3x3 n = uniforms.normalMatrix;
    out[i] = n * in[i];
}
