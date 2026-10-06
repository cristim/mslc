// EXPECT: valid
// DISASM: OpFunctionCall
//
// A constructor with an initialiser list and a body is a helper that builds the
// object: the list assigns its members, then the body runs.
struct B
{
    float x;
    float y;
    B(float a) : x(a) { y = x + 1.0; }
};

kernel void k(device float *out [[buffer(0)]])
{
    B b(2.0);
    out[0] = b.x + b.y;
}
