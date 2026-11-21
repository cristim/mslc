// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// DISASM-MATCH: = OpSLessThan %v2bool
// DISASM-MATCH: = OpSLessThanEqual %v2bool
// DISASM-MATCH: = OpSGreaterThan %v2bool
// DISASM-MATCH: = OpSGreaterThanEqual %v2bool
//
// The four orderings on int vectors are signed and give a bool vector. Each carries its own weight,
// since exactly two of the four hold for any ordering and a plain sum could not tell them apart.
kernel void comparison_int_vector_ordered_signed(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] < b[i]) + 2 * int2(a[i] <= b[i]) + 4 * int2(a[i] > b[i]) + 8 * int2(a[i] >= b[i]);
}
