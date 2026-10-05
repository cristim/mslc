// EXPECT: error give it an initialiser
// A matrix has no single zero constant, so a local declared without a value is
// reported rather than given an OpConstant with no words.
kernel void matrix_local_without_initializer_rejected(device float4 *out [[buffer(0)]],
                                                      uint i [[thread_position_in_grid]])
{
    float4x4 m;
    out[i] = m[0];
}
