// EXPECT: valid
kernel void several_declarators_constructor_arguments(device float *o [[buffer(0)]]) {
  float4 v = float4(1.0), w(2.0);
  o[0] = v.x + w.x;
}
