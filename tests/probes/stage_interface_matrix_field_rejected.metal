// EXPECT: error which mslc does not pass between stages yet
//
// A matrix crossing a stage would take one Location per column, where mslc
// numbers one per field. Apple rejects a matrix in a vertex output as well
// ("invalid return type"), and mslc reports it rather than giving it Locations
// that would overlap the next field's.
struct Out {
    float4 p [[position]];
    float2x2 m;
};

vertex Out stage_interface_matrix_field_rejected(uint vid [[vertex_id]])
{
    Out o;
    o.p = float4(1.0);
    return o;
}
