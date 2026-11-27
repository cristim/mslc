// EXPECT: error a cast to a pointer, reference or template type is not supported
kernel void void_pointer_cast_rejected(device float *o [[buffer(0)]]) {
  void *p = (void *)o;
}
