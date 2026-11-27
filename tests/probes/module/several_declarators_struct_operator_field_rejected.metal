// EXPECT: error an operator overload is not lowered
struct S { int a, operator; };
kernel void several_declarators_struct_operator_field_rejected(device S *s [[buffer(0)]], device float *o [[buffer(1)]]) {
  o[0] = 1.0;
}
