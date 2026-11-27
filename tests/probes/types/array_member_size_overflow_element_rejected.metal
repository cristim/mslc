// EXPECT: error larger than 4 GiB
// The struct size wrapped to 16, so a device buffer of S stepped by 16 bytes.
struct S { float4x4 m[67108864]; float b; };

kernel void array_member_size_overflow_element_rejected(device S *s [[buffer(0)]],
    uint i [[thread_position_in_grid]])
{
    s[i].b = 1.0f;
}
