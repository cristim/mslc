// EXPECT: error needs an initialiser to deduce its type from
// Apple: declaration of variable with deduced type auto requires an initializer.
kernel void auto_local_without_initialiser_rejected(device float *o [[buffer(0)]]) {
  auto x;
  o[0] = 1.0;
}
