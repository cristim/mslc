// EXPECT: valid
// DISASM-MATCH: = OpLogicalNot %v2bool
// DISASM-MATCH: = OpLogicalAnd %v2bool
//
// ! and && on bool vectors act on each component and give a bool vector. They
// used to be typed as a scalar bool, which spirv-val rejects.
kernel void bool_vector_not_and_logical_and(device uint2 *out [[buffer(0)]], constant uint2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 a = bool2(v[i].x > 1u, v[i].y > 1u);
    bool2 r = !a && a;
    uint2 o = uint2(0u);
    if (r.x) { o.x = 1u; }
    out[i] = o;
}
