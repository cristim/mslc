// EXPECT: valid
// DISASM-MATCH: = OpLogicalAnd %v2bool
// DISASM-MATCH: = OpLogicalOr %v2bool
// DISASM-NO-MATCH: OpPhi
// DISASM-NO-MATCH: OpSelectionMerge
//
// && and || on bool vectors are componentwise and evaluate both operands, so
// they stay single instructions; only a scalar operand short-circuits.
kernel void logical_and_of_bool_vectors_evaluates_both(device uint2 *out [[buffer(0)]], constant uint2 *v [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    bool2 a = bool2(v[i].x > 1u, v[i].y > 1u);
    bool2 b = bool2(v[i].y > 1u, v[i].x > 1u);
    bool2 both = a && b;
    bool2 either = a || b;
    uint2 o = uint2(0u);
    o.x = uint(both.x) + 2u * uint(either.y);
    out[i] = o;
}
