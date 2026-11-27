// EXPECT: valid
// A reference to a local is another name for it: stores through r change a.
kernel void local_reference_to_local(device float *o [[buffer(0)]]) {
  float a = 1.0;
  float &r = a;
  r = 2.0;
  r += 3.0;
  o[0] = r + a;
}
