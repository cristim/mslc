// EXPECT: valid
// DISASM-MATCH: = OpINotEqual %v3bool %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
// DISASM-NOT: OpConvertSToF
// DISASM-NOT: OpConvertFToU
//
// An integer vector converts to a bool vector with OpINotEqual against a zero of
// the source type, whatever the signedness.
kernel void bool_vector_from_int_vector_is_not_equal(device uint3 *out [[buffer(0)]], constant int3 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool3 b = bool3(v[i]);
    uint3 r = uint3(0u);
    if (b.x) { r.x = 1u; }
    if (b.z) { r.z = 1u; }
    out[i] = r;
}
