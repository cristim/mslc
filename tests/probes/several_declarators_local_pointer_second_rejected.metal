// EXPECT: error a local of type float* is not lowered yet
// The star after the comma makes only the second declarator a pointer.
kernel void several_declarators_local_pointer_second_rejected(device float *o [[buffer(0)]]) {
  float a = 1.0, *p = &a;
  o[0] = a;
}
