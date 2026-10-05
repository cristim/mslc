// EXPECT: error a column of float2x2 has to be a float2 already
// Apple rejects int2 columns for a float2x2 too; mslc builds the matrix from its
// columns as they are and converts none, so it says so.
kernel void matrix_column_type_rejected(device float2 *out [[buffer(0)]],
                                        device const int2 *columns [[buffer(1)]],
                                        device const float2 *in [[buffer(2)]],
                                        uint i [[thread_position_in_grid]])
{
    float2x2 m = float2x2(columns[0], columns[1]);
    out[i] = m * in[i];
}
