// EXPECT: error recognised but not folded yet
// The star after the comma is read as part of the second declarator.
constant float kA = 1.0, *kP = &kA;
kernel void file_scope_several_declarators_pointer_rejected(device float *o [[buffer(0)]]) {
  o[0] = kA;
}
