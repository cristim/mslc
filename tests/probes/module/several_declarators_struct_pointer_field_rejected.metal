// EXPECT: error declared with several declarators is not lowered yet
// A pointer member in a list is refused rather than lowered as a scalar (#346).
struct S { float a, *p; };
kernel void several_declarators_struct_pointer_field_rejected(device S *s [[buffer(0)]], device float *o [[buffer(1)]]) {
  o[0] = s[0].a;
}
