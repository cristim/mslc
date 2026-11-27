// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_3
// DISASM-MATCH: OpTypeArray %float %uint_5
//
// The names after the first in one declaration are constants too.
constant int a = 1, N = 3;
kernel void array_length_constant_followers(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    const int p = 1, q = 5;
    float x[N];
    float y[q];
    x[0] = float(a); y[4] = 2.0;
    out[i] = x[0] + y[4];
}
