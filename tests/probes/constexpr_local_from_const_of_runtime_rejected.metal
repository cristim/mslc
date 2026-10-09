// EXPECT: error constexpr variable "c" must be initialized by a constant expression
// A const local initialised from a buffer is const, not constant.
kernel void constexpr_local_from_const_of_runtime_rejected(device float *out [[buffer(0)]], device int *ints [[buffer(1)]]) {
  const int n = ints[0];
  constexpr int c = n;
  out[0] = float(c);
}
