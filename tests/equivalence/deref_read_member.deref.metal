struct Pair {
    float4 a;
    float b;
};

kernel void k(device float* out [[buffer(0)]], device const Pair* in [[buffer(1)]])
{
    out[0] = (*in).b + in->b;
}
