// EXPECT: error a scalar cannot be shifted by a vector
kernel void arith_scalar_shifted_by_vector_rejected(device int2 *out [[buffer(0)]],
                                                    device const int *a [[buffer(1)]],
                                                    device const int2 *n [[buffer(2)]],
                                                    uint i [[thread_position_in_grid]])
{
    out[i] = a[i] << n[i];
}
