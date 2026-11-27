// EXPECT: error cannot store through "i"
//
// Apple: cannot assign to variable 'i' with const-qualified type 'const uint'.
kernel void const_builtin_input_store_rejected(device uint *out [[buffer(0)]],
                                         const uint i [[thread_position_in_grid]])
{
    i = 2;
    out[0] = i;
}
