// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=256
// bool & int is an int bitwise and.
kernel void bool_and_int_promotes(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint k [[thread_position_in_grid]])
{
    bool flag = v[k] > 3u;
    int i = int(v[k]);
    float f = float(v[k]);
    out[k] = flag & i;
}
