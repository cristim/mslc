// EXPECT: error points at atomics, which are read and written only by the atomic_*_explicit functions
kernel void atomic_plain_write_rejected(device atomic_uint *c [[buffer(1)]]) {
  c[0] = 1;
}
