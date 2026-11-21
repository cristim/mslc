// EXPECT: error typedef redefinition with different types ('uint' vs 'atomic_uint')
// Apple: "typedef redefinition with different types ('uint' (aka 'unsigned int') vs 'metal::atomic_uint' ...)".
typedef atomic_uint T;
typedef uint T;
kernel void k(device uint *out [[buffer(0)]]) { out[0] = 1u; }
