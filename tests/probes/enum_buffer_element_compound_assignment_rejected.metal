// EXPECT: error a compound assignment does not apply to the enum Mode
enum Mode { A, B, C };
kernel void enum_buffer_element_compound_assignment_rejected(device Mode *bm [[buffer(0)]]) {
  bm[0] += 1;
}
