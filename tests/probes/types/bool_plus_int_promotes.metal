// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=256
// bool + int is an int addition, as in Apple's compiler (the bool is promoted).
kernel void bool_plus_int_promotes(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    out[i] = flag + 1;
}
