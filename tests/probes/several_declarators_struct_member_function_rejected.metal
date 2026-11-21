// EXPECT: error a member function is not lowered yet
struct S { int a, b(); };
kernel void several_declarators_struct_member_function_rejected(device S *s [[buffer(0)]], device float *o [[buffer(1)]]) {
  o[0] = 1.0;
}
