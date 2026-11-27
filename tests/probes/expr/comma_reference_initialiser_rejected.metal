// EXPECT: error after a reference declaration
// Apple: "expected unqualified-id". The comma after a reference initialiser
// starts another declarator; it is not the comma operator.
kernel void comma_reference_initialiser_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0f;
  thread float &r = a, 3.0f;
  o[0] = r;
}
