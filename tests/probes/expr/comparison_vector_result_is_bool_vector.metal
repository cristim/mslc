// EXPECT: valid
// DISASM-MATCH: = OpIEqual %v2bool
// DISASM-MATCH: = OpLogicalAnd %v2bool
// DISASM-MATCH: = OpLogicalNot %v2bool
//
// The result feeds the bool vector operators and a bool2 local.
kernel void comparison_vector_result_is_bool_vector(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    bool2 r = !(a[i] == b[i]) && (a[i] < b[i]);
    out[i] = int2(r);
}
