// EXPECT: error attribute "position" cannot appear more than once on a declaration
struct Out { float4 p [[position, position]]; };
vertex Out field_position_attribute_repeated_rejected() {
  Out o;
  o.p = float4(0.0);
  return o;
}
