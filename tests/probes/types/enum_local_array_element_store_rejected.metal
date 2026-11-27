// EXPECT: error has the enum type Mode, and an integer does not convert to it
enum Mode { A, B, C };
kernel void enum_local_array_element_store_rejected(device int *o [[buffer(0)]]) {
  Mode a[2] = {A, B};
  a[0] = 1;
  o[0] = 1;
}
