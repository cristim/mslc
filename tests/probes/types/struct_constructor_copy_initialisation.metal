// EXPECT: valid
// DISASM: OpFunctionCall
struct B { float x; B(float a) : x(a) {} };
kernel void k(device float *out [[buffer(0)]])
{
    B b = B(3.0);
    out[0] = b.x;
}
