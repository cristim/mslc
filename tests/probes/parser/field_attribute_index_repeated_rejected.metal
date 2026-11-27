// EXPECT: error attribute "attribute" cannot appear more than once on a declaration
struct In { float4 a [[attribute(0), attribute(1)]]; };
vertex float4 field_attribute_index_repeated_rejected(In i [[stage_in]]) {
  return i.a;
}
