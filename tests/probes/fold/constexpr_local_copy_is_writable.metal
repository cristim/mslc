// EXPECT: valid
// DISASM: %float_2
// DISASM: OpFAdd %float
//
// The copy of a constexpr local is a plain local, so it can be stored to.
kernel void constexpr_local_copy_is_writable(device float *out [[buffer(0)]])
{
    constexpr float c = 1.0;
    float d = c;
    d = 2.0;
    out[0] = d + c;
}
