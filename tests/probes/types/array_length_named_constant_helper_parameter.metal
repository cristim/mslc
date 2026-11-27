// EXPECT: valid
//
// A helper parameter "float m[N][N]" decays to a pointer to its first row; the inner length
// is a constant that names N.
constant int N = 2;
float corner(thread float m[N][N]) { return m[N - 1][N - 1]; }

kernel void array_length_named_constant_helper_parameter(device float *out [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    float m[N][N];
    m[1][1] = 7.0;
    out[i] = corner(m);
}
