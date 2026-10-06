// EXPECT: valid
struct S { S() = default; float x; };
struct T { float y; T(); };
kernel void k(device float *out [[buffer(0)]])
{
    S s;
    s.x = 1;
    out[0] = s.x;
}
