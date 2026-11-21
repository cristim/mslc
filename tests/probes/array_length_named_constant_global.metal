// EXPECT: valid
// DISASM-MATCH: OpTypeArray %float %uint_3
// DISASM-MATCH: OpTypeArray %_arr_float_uint_3[_0-9]* %uint_4
//
// An array length may name a file-scope constant and do arithmetic on it. Apple takes both
// "constexpr constant" and "constant" (a constant-space variable is const).
constexpr constant int N = 3;
constant int M = N + 1;

kernel void array_length_named_constant_global(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float a[N];
    float b[M][N];
    for (int k = 0; k < N; ++k) { a[k] = float(k); }
    b[M - 1][N - 1] = a[2];
    out[i] = a[1] + b[3][2];
}
