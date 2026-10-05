// EXPECT: error this return's value is another struct
//
// The returned value is split into the Output variables field by field, so it
// has to be the struct the function is declared to return. Apple rejects this
// source too. A struct read straight from a buffer ("return vertices[vid];") is
// valid MSL and reaches the same diagnostic, because its SPIR-V type carries the
// buffer's member offsets and mslc does not copy it into the plain form yet.
struct Out { float4 p [[position]]; };
struct Other { float4 p; };

vertex Out returned_struct_of_another_type_rejected(uint vid [[vertex_id]])
{
    Other o;
    o.p = float4(1.0);
    return o;
}
