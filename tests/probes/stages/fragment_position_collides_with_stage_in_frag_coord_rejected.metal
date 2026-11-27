// EXPECT: error parameter "q" is [[frag_coord]], which collides
//
// The same cross-source collision as
// fragment_position_collides_with_stage_in_rejected, but the direct
// parameter uses [[frag_coord]] instead of [[position]]. The collision guard
// is keyed on the SPIR-V builtin, not the MSL attribute name, so this pins
// that mixing the two spellings is caught, not just two uses of the same
// one.
struct In { float4 pos [[position]]; };

fragment float4 fragment_position_collides_with_stage_in_frag_coord_rejected(In in [[stage_in]],
    float4 q [[frag_coord]])
{
    return in.pos * q;
}
