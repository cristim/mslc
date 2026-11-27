// EXPECT: error a cast to a pointer, reference or template type is not supported
//
// Apple: "static_cast from 'device uint *' to 'int *' is not allowed". The star
// binds after the specifier run, so this exits through the parseCast-style
// pointer reject rather than the address-space one.
kernel void static_cast_to_bare_pointer_rejected(device float* out [[buffer(0)]], device uint* in [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    int* p = static_cast<int*>(in);
    out[i] = float(p[0]);
}
