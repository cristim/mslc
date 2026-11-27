// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=256
// float * bool multiplies by the bool as 0 or 1.
kernel void float_times_bool_promotes(device float *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint k [[thread_position_in_grid]])
{
    bool flag = v[k] > 3u;
    int i = int(v[k]);
    float f = float(v[k]);
    out[k] = f * flag;
}
