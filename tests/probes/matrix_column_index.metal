// EXPECT: valid
// DISASM-MATCH: OpAccessChain %_ptr_Function_v4float %[0-9]+ %
//
// Indexing a matrix names a column, so the access chain points at a float4 and
// not at another float4x4.
kernel void matrix_column_index(device float4 *out [[buffer(0)]],
                                device const float4x4 *m [[buffer(1)]],
                                uint i [[thread_position_in_grid]])
{
    float4x4 local = m[0];
    out[i] = local[i];
}
