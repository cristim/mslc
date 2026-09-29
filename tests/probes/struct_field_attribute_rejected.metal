// EXPECT: error builtin attribute "frag_coord" is not valid on a struct field
//
// A struct field's attribute list is scanned, not skipped. Before it was read,
// the '[' of "[[frag_coord]]" reached parseType and this source failed with
// 'expected a type, found "["', which said nothing about the attribute.
//
// A builtin that is valid on a parameter is not valid on a field: Metal gives a
// field only [[position]] and [[attribute(n)]], and naming the difference beats
// accepting the list and dropping an attribute that would change the interface.
struct Vertex
{
    float4 position [[position]];
    float4 color;
    float4 uv [[frag_coord]];
};

kernel void struct_field_attribute_rejected(device float* out [[buffer(0)]],
                                           uint i [[thread_position_in_grid]])
{
    out[i] = 1.0;
}
