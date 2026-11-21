// EXPECT: valid
// RUN: grid=64 local=8 cases=3 elems=256
// -flag is an int (0 or -1).
kernel void bool_negate_promotes(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    out[i] = -flag;
}
