// EXPECT: valid
// DISASM-MATCH: = OpIEqual %v2bool
// DISASM-MATCH: = OpINotEqual %v2bool
//
// == and != on two int vectors compare componentwise and give a bool vector.
kernel void comparison_int_vector_equal(device int2 *out [[buffer(0)]],
    constant int2 *a [[buffer(1)]], constant int2 *b [[buffer(2)]],
    uint i [[thread_position_in_grid]])
{
    bool2 e = a[i] == b[i];
    bool2 n = a[i] != b[i];
    out[i] = int2(e) + int2(n);
}
