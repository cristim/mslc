// EXPECT: error would have the enum type E, which is not lowered yet
// Apple deduces E, and then rejects "e = 1"; mslc would deduce an int and accept it.
enum E { A, B };
kernel void auto_local_enum_rejected(device float *o [[buffer(0)]]) {
  auto e = A;
  o[0] = e;
}
