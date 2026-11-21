// EXPECT: error redefinition of "kA"
constant float kA = 1.0, kA = 2.0;
kernel void file_scope_several_declarators_redefinition_rejected(device float *o [[buffer(0)]]) {
  o[0] = kA;
}
