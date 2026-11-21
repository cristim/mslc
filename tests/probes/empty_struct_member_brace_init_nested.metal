// EXPECT: valid
// RUN: grid=64 local=8 cases=3
// An empty struct as a member, brace-initialised with {} or explicit braces, nested, and
// read back out of the outer struct. The empty value is built without a constituent list.
// DISASM: OpTypeStruct %uint
struct Emp {};
struct In { Emp a; int x; };
struct Outer { In i; Emp b; float c; };
float h(Emp e) { return 7.0f; }
kernel void k(device float* out [[buffer(0)]])
{
    Outer o{{{}, 4}, {}, 1.5f};
    Outer p{In{Emp{}, 5}, Emp{}, 2.5f};
    Outer q = p;
    q.i.x = 8;
    q.b = o.b;
    out[0] = o.i.x;
    out[1] = o.c;
    out[2] = p.i.x;
    out[3] = p.c;
    out[4] = q.i.x + p.i.x;
    out[5] = h(o.i.a);
}
