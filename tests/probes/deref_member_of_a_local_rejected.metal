// EXPECT: error "s" is not a pointer, so unary '*' cannot dereference it
//
// A struct local is a value, so "(*s).b" is indirection through a non-pointer in
// Apple's compiler. Without the check it reads as "s[0].b".
struct Pair {
    float4 a;
    float b;
};

kernel void deref_member_of_a_local_rejected(device float* out [[buffer(0)]])
{
    Pair s;
    s.b = 1.0;
    out[0] = (*s).b;
}
