// EXPECT: error points at atomics, which are read and written only by the atomic_*_explicit functions
// Apple: "no viable conversion from device atomic_uint".
kernel void atomic_plain_read_rejected(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  o[0] = c[0];
}
