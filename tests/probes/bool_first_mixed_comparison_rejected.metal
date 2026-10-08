// EXPECT: error a bool is not lowered yet
kernel void mixed(device int *out [[buffer(0)]], constant int *in [[buffer(1)]]) {
    bool a = in[0] != 0;
    out[0] = int(a == in[1]);
}
