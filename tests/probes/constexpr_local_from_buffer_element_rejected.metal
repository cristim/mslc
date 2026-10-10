// EXPECT: error constexpr variable "c" must be initialized by a constant expression
// Apple: "constexpr variable 'c' must be initialized by a constant expression".
kernel void constexpr_local_from_buffer_element_rejected(device float *out [[buffer(0)]], device int *ints [[buffer(1)]]) {
  constexpr int c = ints[0];
  out[0] = float(c);
}
