// EXPECT: valid
// DISASM-MATCH: = OpCompositeConstruct %v2int
// DISASM-MATCH: = OpSLessThan %v2bool
//
// A scalar beside a vector is broadcast to its component type, on either side.
kernel void comparison_vector_scalar_broadcasts(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]], constant int *s [[buffer(3)]],
    uint i [[thread_position_in_grid]])
{
    out[i] = int2(a[i] < s[i]) + int2(s[i] < b[i]);
}
