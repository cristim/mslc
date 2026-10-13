// EXPECT: error an element of the initializer list has the enum type Mode
enum Mode { A, B, C };
kernel void enum_brace_array_element_integer_rejected(device int *o [[buffer(0)]]) {
  Mode a[2] = { 1, A };
  o[0] = 1;
}
