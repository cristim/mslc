// EXPECT: error initialised from a braced list is not lowered yet
// Apple accepts it as a different type than the element.
kernel void auto_local_braced_list_rejected(device float *o [[buffer(0)]]) {
  auto x = {1};
  o[0] = 1.0;
}
