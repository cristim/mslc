// EXPECT: valid
// DISASM-NOT: OpFwidth
float unused(float x) { return fwidth(x); }
kernel void main() {}
