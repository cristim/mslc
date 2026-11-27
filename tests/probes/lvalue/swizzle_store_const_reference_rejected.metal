// EXPECT: error assigning to ".x" of "h", which is const or in constant memory
struct Holder {
    float4 v;
};

kernel void swizzle_store_const_reference_rejected(device float4 *out [[buffer(0)]], const device Holder &h [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    h.v.x = 1.0;
    out[i] = h.v;
}
