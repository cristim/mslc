// EXPECT: error field "s" of "In" is a short, which mslc does not pass between stages yet
//
// A 16-bit integer crossing a stage needs storageInputOutput16, and unlike a
// half it has no exact 32-bit form that keeps its signedness without a
// conversion mslc does not make here, so it is reported.
struct In {
    float4 p [[position]];
    short s;
};

fragment float4 stage_interface_narrow_integer_rejected(In in [[stage_in]])
{
    return float4(1.0);
}
