struct Pair {
    float4 a;
    float b;
};

kernel void k(device Pair* out [[buffer(0)]], device const float* in [[buffer(1)]])
{
    out[0].b = in[0];
}
