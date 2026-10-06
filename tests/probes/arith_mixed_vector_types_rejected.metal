// EXPECT: error an operator takes two vectors of the same type
//
// Apple rejects int2 + uint2 ("implicit conversions between vector types are not
// permitted"); mslc converted one side to the other's type.
kernel void arith_mixed_vector_types_rejected(device int2 *out [[buffer(0)]],
                                              device const int2 *a [[buffer(1)]],
                                              device const uint2 *b [[buffer(2)]],
                                              uint i [[thread_position_in_grid]])
{
    out[i] = a[i] + b[i];
}
