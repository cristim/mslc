// EXPECT: error takes one [[stage_in]] parameter
//
// One [[stage_in]] per function; Apple rejects a second too. mslc reports it
// rather than numbering the second struct's Locations after the first's.
struct In {
    float4 p [[position]];
    float4 c;
};

fragment float4 two_stage_in_parameters_rejected(In a [[stage_in]], In b [[stage_in]])
{
    return a.c;
}
