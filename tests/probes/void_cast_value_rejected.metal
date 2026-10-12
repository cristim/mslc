// EXPECT: error a cast to void has no value
// Apple: "cannot initialize a variable of type float with an rvalue of type void".
kernel void void_cast_value_rejected(device float *o [[buffer(0)]]) {
  float y = (void)1.0;
  o[0] = y;
}
