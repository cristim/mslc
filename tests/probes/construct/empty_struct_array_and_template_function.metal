// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// An array of empty structs, elements read and assigned, and an empty struct passed to a
// function template instance.
// DISASM: OpTypeStruct %uint
template <typename T> float t(T e) { return 9.0f; }
struct Emp {};
float h(Emp e) { return 8.0f; }
kernel void k(device float* out [[buffer(0)]])
{
    Emp a[3];
    Emp b[2] = {{}, {}};
    Emp c[2] = {Emp{}, Emp{}};
    c[1] = a[2];
    out[0] = h(a[1]);
    out[1] = h(b[0]);
    out[2] = h(c[1]);
    out[3] = t(c[0]);
    out[4] = t<Emp>(b[1]);
}
