// EXPECT: error after type "float"
// Apple: "expected ';' after expression". A comma list is not a declaration, so
// a type name after the comma is not one.
kernel void comma_declaration_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0f;
  a = 2.0f, float b = 3.0f;
  o[0] = a + b;
}
