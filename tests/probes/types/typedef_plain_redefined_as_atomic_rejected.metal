// EXPECT: error typedef redefinition with different types ('atomic_uint' vs 'uint')
// Apple: "typedef redefinition with different types ('metal::atomic_uint' ... vs 'uint' ...)".
typedef uint T;
typedef atomic_uint T;
kernel void k(device uint *out [[buffer(0)]]) { out[0] = 1u; }
