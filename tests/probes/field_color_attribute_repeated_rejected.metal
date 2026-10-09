// EXPECT: error attribute "color" cannot appear more than once on a declaration
// Apple: "attribute 'color' cannot appear more than once on a declaration".
struct Out { float4 a [[color(0), color(1)]]; };
fragment Out field_color_attribute_repeated_rejected() {
  Out o;
  o.a = float4(0.0);
  return o;
}
