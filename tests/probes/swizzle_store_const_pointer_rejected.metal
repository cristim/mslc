// EXPECT: error assigning to ".x" of "table", which is const or in constant memory
//
// Apple: "read-only variable is not assignable".
kernel void swizzle_store_const_pointer_rejected(device float4 *out [[buffer(0)]], device const float4 *table [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    table[i].x = 1.0;
    out[i] = table[i];
}
