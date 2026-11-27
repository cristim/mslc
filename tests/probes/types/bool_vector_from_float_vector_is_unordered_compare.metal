// EXPECT: valid
// DISASM-MATCH: = OpFUnordNotEqual %v2bool %[_0-9a-zA-Z]+ %[_0-9a-zA-Z]+[^ _0-9a-zA-Z]
// DISASM-NOT: OpFOrdNotEqual
// DISASM-NOT: OpConvertFToU
//
// A numeric vector converts to a bool vector as "not equal to zero", and a NaN is
// not equal to zero, so the compare is the unordered one.
kernel void bool_vector_from_float_vector_is_unordered_compare(device uint2 *out [[buffer(0)]], constant float2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 b = bool2(v[i]);
    uint2 r = uint2(0u);
    if (b.x) { r.x = 1u; }
    if (b.y) { r.y = 1u; }
    out[i] = r;
}
