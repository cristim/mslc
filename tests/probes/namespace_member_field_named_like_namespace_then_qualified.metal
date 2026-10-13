// EXPECT: valid
// DISASM-MATCH: OpIAdd %int %[_0-9a-zA-Z]+ %int_11[_0-9]*
//
// A struct's own field named like a namespace, used through the implicit
// this inside a const member function: "A + A::x" with A a member field.
// This pins parsePrimary's implicit-this member-field shortcut specifically;
// deleting only that half makes this probe, and only this probe, fail.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; }
struct S {
    int A;
    int get() const { return A + A::x; }
};
kernel void namespace_member_field_named_like_namespace_then_qualified(device int *o [[buffer(0)]])
{
    S s; s.A = 22;
    o[0] = s.get();
}
