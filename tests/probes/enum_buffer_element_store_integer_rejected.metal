// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_buffer_element_store_integer_rejected(device Mode *bm [[buffer(0)]]) {
  bm[0] = 1;
}
