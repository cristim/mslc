// EXPECT: error a bool is not lowered yet
kernel void ordering(device int *out [[buffer(0)]]) { out[0] = int(false < true); }
