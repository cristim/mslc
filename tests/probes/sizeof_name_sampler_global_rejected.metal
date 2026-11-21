// EXPECT: error worked out after parsing
// Apple gives 8 for sizeof of a file-scope sampler. A resource has no recorded type, so an array
// length from its size is refused.
constexpr sampler smp(coord::normalized);
kernel void sizeof_name_sampler_global_rejected(device float *out [[buffer(0)]])
{
    float a[sizeof(smp)];
    out[0] = a[0];
}
