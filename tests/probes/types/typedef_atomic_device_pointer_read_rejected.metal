// EXPECT: error points at atomics, which are read and written only by
// Apple: "assigning to 'uint' from incompatible type 'device Counter' (aka 'device _atomic<unsigned int>')".
typedef atomic_uint Counter;
kernel void k(device uint *out [[buffer(0)]], device Counter *c [[buffer(1)]]) {
  out[0] = c[0];
}
