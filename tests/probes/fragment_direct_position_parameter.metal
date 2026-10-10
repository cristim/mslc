// EXPECT: valid
// DISASM: FragCoord
//
// A fragment function can take [[position]] directly, with no [[stage_in]]
// struct: the same window-space position FragCoord already carries on a
// struct field. The return reads p so a compiler that silently zeroed it
// would fail this probe, not just a bare validity check.
fragment float4 fragment_direct_position_parameter(float4 p [[position]])
{
    return p * 0.5;
}
