// EXPECT: error worked out after parsing
// Apple gives 8 for sizeof of a sampler parameter. A resource has no recorded type, so an array
// length from its size is refused.
kernel void sizeof_name_sampler_parameter_rejected(device float *out [[buffer(0)]],
    sampler smp [[sampler(0)]])
{
    float a[sizeof(smp)];
    out[0] = a[0];
}
