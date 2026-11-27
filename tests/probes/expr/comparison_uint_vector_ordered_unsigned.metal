// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DISASM-MATCH: = OpULessThan %v3bool
// DISASM-MATCH: = OpUGreaterThanEqual %v3bool
//
// uint vectors compare unsigned, in any width. The two results are weighted 1 and 2, because they
// are complements and a plain sum is always 1.
kernel void comparison_uint_vector_ordered_unsigned(device int2 *out [[buffer(0)]],
    constant uint3 *a [[buffer(1)]], constant uint3 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    int3 r = int3(a[i] < b[i]) + 2 * int3(a[i] >= b[i]);
    out[i] = int2(r.x, r.z);
}
