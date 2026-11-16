// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// Program-scope constants of an empty struct: a plain one, an array of them and one as a
// member of a constant struct. Each is the null value of the one-member value form, not an
// OpConstantComposite with no constituents.
// DISASM: OpTypeStruct %uint
struct Emp {};
struct Holder { Emp e; float c; };
constant Emp ce = Emp{};
constant Emp cb = {};
constant Emp ca[2] = {Emp{}, {}};
constant Holder ch = {.e = Emp{}, .c = 2.5f};
float h(Emp e) { return 6.0f; }
kernel void k(device float* out [[buffer(0)]])
{
    out[0] = h(ce);
    out[1] = h(cb);
    out[2] = h(ca[1]);
    out[3] = ch.c + h(ch.e);
}
