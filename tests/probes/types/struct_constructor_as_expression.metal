// EXPECT: valid
// DISASM: OpFunctionCall
struct B { float x; B(float a) : x(a * 2.0) {} };
float read(B b) { return b.x; }
kernel void k(device float *out [[buffer(0)]])
{
    out[0] = read(B(3.0));
}
