// EXPECT: error parameter "p" is [[position]], which collides
//
// A [[stage_in]] struct with its own [[position]] field, plus a separate
// direct [[position]] parameter: both lower to a BuiltIn FragCoord Input
// variable, which spirv-val rejects as a duplicate interface variable
// (VUID-StandaloneSpirv-OpEntryPoint-09658). mslc reports the collision
// itself instead of emitting the module and letting validation name it.
struct In { float4 pos [[position]]; };

fragment float4 fragment_position_collides_with_stage_in_rejected(In in [[stage_in]],
    float4 p [[position]])
{
    return in.pos * p;
}
