// EXPECT: error assigning to ".x" of a value that is not a vector
kernel void swizzle_store_of_a_scalar_rejected(device float *out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    float f = 0.0;
    f.x = 1.0;
    out[i] = f;
}
