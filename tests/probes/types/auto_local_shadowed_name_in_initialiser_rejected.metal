// EXPECT: error cannot appear in its own initializer
// Apple: variable x declared with deduced type auto cannot appear in its own initializer. An outer x must not be read instead.
kernel void auto_local_shadowed_name_in_initialiser_rejected(device float *o [[buffer(0)]]) {
  float x = o[0];
  {
    auto x = x + 1;
    o[1] = x;
  }
}
