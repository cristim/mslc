// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_3
//
// int(3) initialises an int as 3, and so does a brace list.
constant int N = int(3);
kernel void array_length_constant_same_type_constructor(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    const int L{3};
    float a[N];
    float c[L];
    a[0] = 1.0; c[2] = 3.0;
    out[i] = a[0] + c[2];
}
