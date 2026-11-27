// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_5
//
// A local const or constexpr integer with a constant initialiser is a constant; a const
// initialised from another constant is one too.
kernel void array_length_named_constant_local(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    constexpr int a_len = 2;
    const int b_len = a_len + 3;
    float v[b_len];
    for (int k = 0; k < b_len; ++k) { v[k] = float(k); }
    out[i] = v[4];
}
