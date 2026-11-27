// EXPECT: error assigning to ".x" of "table", which is const or in constant memory
//
// const before the address space is the same declaration.
kernel void swizzle_store_const_first_pointer_rejected(device float4 *out [[buffer(0)]], const device float4 *table [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    table[i].x = 1.0;
    out[i] = table[i];
}
