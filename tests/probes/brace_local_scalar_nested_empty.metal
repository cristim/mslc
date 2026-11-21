// EXPECT: valid
// DIFFCHECK: writes-zero
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) { float x { {} }; out[0] = x; }
