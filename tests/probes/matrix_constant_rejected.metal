// EXPECT: error a float2x2 constant is not lowered yet
// The constant folder builds vectors and structs; a matrix constant would be read
// as a vector of its row count.
constant float2x2 rotation = { float2(1.0, 0.0), float2(0.0, 1.0) };

kernel void matrix_constant_rejected(device float2 *out [[buffer(0)]],
                                     uint i [[thread_position_in_grid]])
{
    out[i] = rotation * float2(1.0);
}
