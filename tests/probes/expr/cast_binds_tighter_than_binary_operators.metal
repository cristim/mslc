// EXPECT: valid
// DISASM: = OpFDiv %float
// DISASM-NOT: = OpSDiv
//
// `(float)n / 2` casts n, then divides as floats; a cast over the whole quotient
// would be an integer division.
kernel void cast_binds_tighter_than_binary_operators(device float* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int n = int(i);
    out[i] = (float)n / 2;
}
