// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_dereference_store_integer_rejected(device Mode *bm [[buffer(0)]]) {
  *bm = 1;
}
