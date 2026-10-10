// EXPECT: valid
// A bool member is a byte in the buffer; reading and writing it through a reference
// to the struct has to use the same byte mapping as the direct access.
struct S { bool b; float f; };
kernel void local_reference_struct_bool_member(device float *o [[buffer(0)]], device S *s [[buffer(1)]]) {
  device S &r = s[0];
  o[0] = r.b ? 1.0 : 0.0;
  r.b = !r.b;
}
