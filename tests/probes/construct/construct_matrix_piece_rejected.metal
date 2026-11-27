// EXPECT: error float4 is built from scalars and vectors, not from a matrix or a struct
//
// Apple reports "no matching constructor" for a matrix piece.
kernel void construct_matrix_piece_rejected(device float4 *out [[buffer(0)]], constant float2x2 *m [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = float4(m[i], 1.0, 1.0);
}
