// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=256
// A runtime bool promotes to int under every arithmetic, shift and unary operator, and to float
// beside a float.
kernel void bool_runtime_arithmetic_mixed(device float *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool t = v[i] > 3u;
    bool u = v[i + 1] > 3u;
    int n = int(v[i] & 7u);
    int k = t * 5 - u + (t << 2) + (n << t);
    k += t;
    k -= ~u;
    out[i * 4 + 0] = float(k);
    out[i * 4 + 1] = float(t + u) + (t - 0.25f);
    out[i * 4 + 2] = float(+t + -u + (t ^ u));
    out[i * 4 + 3] = float(n % (t + 1) + 9 / (u + 1));
}
