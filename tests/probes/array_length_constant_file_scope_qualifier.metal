// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_3
// DISASM-NOT: OpTypeArray %float %uint_2
//
// "::N" names the file-scope constant (3) beside a local constexpr N that is 2.
constant int N = 3;
kernel void array_length_constant_file_scope_qualifier(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    constexpr int N = 2;
    float a[::N];
    a[2] = float(N);
    out[i] = a[2];
}
