// EXPECT: valid
// DISASM-NOT: OpFunctionCall
//
// A struct with an empty default constructor and a parameterised one: "S s;"
// takes the empty one and calls nothing.
struct S { float x; S() {} S(float a) : x(a) {} };
kernel void k(device float *out [[buffer(0)]])
{
    S s;
    s.x = 1.0;
    out[0] = s.x;
}
