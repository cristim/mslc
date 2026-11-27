// EXPECT: error worked out after parsing
// Apple takes sizeof(s.a) as 16. A member is not a declared name the parser records, so it is
// refused rather than sized.
struct S { float4 a; float b; };
kernel void sizeof_name_member_array_length_rejected(device float *out [[buffer(0)]])
{
    S s;
    float a[sizeof(s.a)];
    out[0] = a[0] + s.b;
}
