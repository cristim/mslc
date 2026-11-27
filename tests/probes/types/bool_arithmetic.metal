// EXPECT: valid
// RUN: grid=16 local=8 cases=3 elems=256
//
// The bool operand is promoted to int ([conv.prom]), as Apple's compiler does. (This probe pinned the
// refusal until the promotion was lowered.)
kernel void bool_arithmetic(device int *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool flag = v[i] > 3u;
    out[i] = flag + 1;
}
