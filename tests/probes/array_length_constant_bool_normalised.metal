// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_1
//
// "const bool B = 5" is true, so the length is 1 (Apple).
kernel void array_length_constant_bool_normalised(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    const bool B = 5;
    float a[B];
    a[0] = 1.0;
    out[i] = a[0];
}
