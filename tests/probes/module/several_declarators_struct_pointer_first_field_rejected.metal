// EXPECT: error declared with several declarators is not lowered yet
struct S { device float *p, q; };
kernel void several_declarators_struct_pointer_first_field_rejected(device S *s [[buffer(0)]], device float *o [[buffer(1)]]) {
  o[0] = s[0].q;
}
