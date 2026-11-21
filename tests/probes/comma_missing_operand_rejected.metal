// EXPECT: error in an expression
// Apple: "expected expression". A comma needs an operand on each side.
kernel void comma_missing_operand_rejected(device float *o [[buffer(0)]]) {
  float a = (1.0f, );
  o[0] = a;
}
