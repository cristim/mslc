// EXPECT: error parameter "q" is [[position]], which collides
//
// Two direct [[position]] parameters on the same fragment function: both
// lower to a BuiltIn FragCoord Input variable, which the collision guard
// rejects. [[position]] rather than [[frag_coord]] here, since Apple's MSL
// has no [[frag_coord]] attribute at all (it is mslc's own invented spelling,
// already rejected on struct fields by struct_field_attribute_rejected.metal),
// so this is the spelling a real Metal source would actually write.
fragment float4 fragment_two_direct_position_parameters_rejected(float4 p [[position]],
    float4 q [[position]])
{
    return p * q;
}
