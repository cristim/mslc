// EXPECT: valid
constant float K = 1.0;

kernel void store_local_shadowing_file_scope_constant_is_writable(device float *out [[buffer(0)]])
{
    float K = 2.0;
    K = 3.0;
    K += 1.0;
    out[0] = K;
}
