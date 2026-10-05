// EXPECT: error "x" is not a pointer, so unary '*' cannot dereference it
//
kernel void deref_of_a_local_rejected(device float* out [[buffer(0)]])
{
    float x = 1.0;
    out[0] = *x;
}
