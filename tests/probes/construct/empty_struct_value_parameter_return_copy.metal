// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// An empty struct passed, returned, copied and assigned by value. Mesa's SPIR-V front end
// rejects a zero-member struct as a parameter, so the value form keeps one placeholder member.
// DISASM: OpTypeStruct %uint
// DISASM-MATCH: = OpFunctionParameter %_struct_[0-9]+
struct Emp {};
float h(Emp e) { return 3.0f; }
float r(thread Emp& e) { return 4.0f; }
Emp make() { Emp e; return e; }
kernel void k(device float* out [[buffer(0)]])
{
    Emp e;
    Emp f = make();
    Emp g;
    g = f;
    out[0] = h(e);
    out[1] = r(g);
    out[2] = h(make());
    out[3] = h(f);
}
