// EXPECT: error constexpr variable "c" must be initialized by a constant expression
kernel void constexpr_local_from_thread_id_rejected(device float *out [[buffer(0)]], uint gid [[thread_position_in_grid]]) {
  constexpr uint c = gid;
  out[0] = float(c);
}
