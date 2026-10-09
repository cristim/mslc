// EXPECT: valid
// A helper passes its own reference parameter on to another helper.
void set3(thread float &x) { x = 3.0; }
void forward(thread float &y) { set3(y); y += 1.0; }
kernel void helper_reference_parameter_forwarded(device float *o [[buffer(0)]]) {
  float a = 0.0;
  forward(a);
  o[0] = a;
}
