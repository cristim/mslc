// EXPECT: valid
// DISASM: OpLogicalEqual %v2bool
kernel void comparison_bool_vector(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    bool2 x = a[i] == b[i];
    out[i] = int2(x == x);
}
