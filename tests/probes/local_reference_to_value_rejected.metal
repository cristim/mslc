// EXPECT: error has to be bound to a variable, an element or a member
kernel void local_reference_to_value_rejected(device float *o [[buffer(0)]]) {
  const float &r = 1.0;
  o[0] = r;
}
