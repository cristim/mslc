// EXPECT: error the struct Other is used where the struct Out is expected
//
// The returned value is split into the Output variables field by field, so it
// has to be the struct the function is declared to return. Apple rejects this
// source too. A struct read straight from a buffer ("return vertices[vid];") is
// the same struct in another layout, and is copied member by member.
struct Out { float4 p [[position]]; };
struct Other { float4 p; };

vertex Out returned_struct_of_another_type_rejected(uint vid [[vertex_id]])
{
    Other o;
    o.p = float4(1.0);
    return o;
}
