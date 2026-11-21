// EXPECT: error typedef redefinition with different types ('atomic_uint' vs 'atomic_int')
// Apple: "typedef redefinition with different types ('_atomic<metal::uint>' vs '_atomic<int>')".
typedef atomic_int T;
typedef atomic_uint T;
kernel void k(device uint *out [[buffer(0)]]) { out[0] = 1u; }
