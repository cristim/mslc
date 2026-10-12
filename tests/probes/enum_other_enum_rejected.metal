// EXPECT: error a value of the enum Other does not convert to it
enum Mode { A, B };
enum Other { X, Y };
kernel void enum_other_enum_rejected(device int *o [[buffer(0)]]) {
  Mode m = X;
  o[0] = 1;
}
