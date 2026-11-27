// EXPECT: error needs an initialiser
constant float kA = 1.0, kB;
kernel void file_scope_several_declarators_need_initialiser_rejected(device float *o [[buffer(0)]]) {
  o[0] = kA;
}
