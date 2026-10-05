// EXPECT: valid
// DISASM-MATCH: = OpSelect %float %[_0-9a-zA-Z]+ %float_1 %float_0[^_0-9a-zA-Z]
// DISASM-NOT: OpConvertUToF
//
// The scalar case: float(bool) is 1.0 or 0.0.
kernel void float_from_bool_is_select(device float *out [[buffer(0)]], constant uint *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool b = v[i] > 3u;
    out[i] = float(b);
}
