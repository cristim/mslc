// EXPECT: error larger than 4 GiB
// A 4 GiB array member wrapped the 32-bit offset arithmetic, so the member after
// it came out at Offset 0 and the module still validated.
struct S { float a[1073741824]; float b; };

kernel void array_member_size_overflow_rejected(device float *out [[buffer(0)]],
    constant S &s [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    out[i] = s.b;
}
