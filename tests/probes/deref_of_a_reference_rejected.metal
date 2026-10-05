// EXPECT: error "scale" is not a pointer, so unary '*' cannot dereference it
//
// A "constant float &" is the value itself, so Apple reports the '*' as
// indirection through a non-pointer.
kernel void deref_of_a_reference_rejected(device float* out [[buffer(0)]],
                                          constant float& scale [[buffer(1)]])
{
    out[0] = *scale;
}
