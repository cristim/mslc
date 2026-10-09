// EXPECT: valid
// A device reference to a buffer element and to a struct element is stored through.
struct S { float a; float b; };
kernel void local_reference_store_to_buffer_element(device float *o [[buffer(0)]], device S *s [[buffer(1)]]) {
  device float &r = o[1];
  r += 1.0;
  device S &t = s[0];
  t.a = 2.0;
  t.b = t.a + r;
}
