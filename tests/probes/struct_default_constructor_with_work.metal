// EXPECT: valid
// DISASM: OpFunctionCall
//
// "E e;" runs a default constructor that has an initialiser list.
struct E { int x; E() : x(5) {} };
kernel void k(device int *out [[buffer(0)]])
{
    E e;
    out[0] = e.x;
}
