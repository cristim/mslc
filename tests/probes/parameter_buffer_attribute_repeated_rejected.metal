// EXPECT: error attribute "buffer" cannot appear more than once on a declaration
kernel void parameter_buffer_attribute_repeated_rejected(device float *p [[buffer(0), buffer(1)]]) {
  p[0] = 1.0;
}
