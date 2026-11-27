// EXPECT: valid
// DISASM: FragCoord
//
// A direct [[position]] parameter coexists with a [[stage_in]] struct that
// has no [[position]] field of its own: the two paths each declare their own
// Input variable and neither collides with the other.
struct In { float4 c [[user(locn0)]]; };

fragment float4 fragment_direct_position_parameter_with_stage_in(In in [[stage_in]],
    float4 p [[position]])
{
    return in.c * 0.5 + p * 0.5;
}
