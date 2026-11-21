// EXPECT: valid
// DIFFCHECK-RANGE: in 0 1
// DISASM: OpIEqual
kernel void mixed(device int *out [[buffer(0)]], constant int *in [[buffer(1)]]) {
    bool a = in[0] != 0;
    out[0] = int(a == in[1]);
}
