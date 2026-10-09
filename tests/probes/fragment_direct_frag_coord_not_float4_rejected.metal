// EXPECT: error parameter "q" is [[frag_coord]], which has to be a float4
//
// The same float4 type check as fragment_direct_position_not_float4_rejected,
// but through the [[frag_coord]] spelling instead of [[position]]. The check
// is keyed on the SPIR-V builtin (BuiltIn FragCoord), not the MSL attribute
// name, so this pins that it is not accidentally narrowed to only one of the
// two spellings that reach it.
fragment float4 fragment_direct_frag_coord_not_float4_rejected(float2 q [[frag_coord]])
{
    return float4(q, 0.0, 1.0);
}
