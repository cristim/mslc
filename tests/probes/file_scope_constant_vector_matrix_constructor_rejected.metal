// EXPECT: error initialised with a braced list of its components
//
// Apple rejects a matrix constructor as a float4 initialiser; so does mslc, not as a column.
constant float4 c = float4x4(1);
kernel void file_scope_constant_vector_matrix_constructor_rejected(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = c.x;
}
