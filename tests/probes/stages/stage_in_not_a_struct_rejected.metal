// EXPECT: error mslc lowers a [[stage_in]] struct and no other type
//
// Only a struct is lowered as [[stage_in]]; Apple rejects anything else as well.
fragment float4 stage_in_not_a_struct_rejected(float4 c [[stage_in]])
{
    return c;
}
