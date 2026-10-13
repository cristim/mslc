// EXPECT: valid
// DISASM-MATCH: OpStore %[_0-9a-zA-Z]+ %int_11[_0-9]* Aligned
// What does not conflict, all accepted by Apple: a reopened namespace, the same name in
// another namespace, a local named like a namespace, a function with other parameters
// beside a using declaration in either order (a thread reference, a const thread reference
// and a thread pointer are other parameter types than a value), a struct tag beside a
// using-declared variable in either order, the same using declaration twice, two using
// declarations of functions that are never called, a using declaration of a name that is
// also a namespace declared before it, an enumerator declared after a using declaration
// (ambiguous only where used), a variable beside a directive's member, a member
// function named like a using declaration, a local, a parameter and an entry-point
// parameter named like two using-declared functions (a declaration is not a use; then
// the local is what the name means), and a using-declared struct beside using-declared
// functions of its name.
#include <metal_stdlib>
using namespace metal;
namespace A { constant int x = 11; int f() { return 11; } int g(thread int& a) { return 1; }
  constant int y = 3; constant int z = 4; constant int w = 5; struct S { int q; };
  int h2(const thread int& a) { return 1; } int h3(thread int* p) { return p[0]; }
  int k() { return 1; } constant int n1 = 1; int n2() { return 2; } }
namespace A { constant int x2 = 2; }
namespace B { int k() { return 2; } int S2() { return 2; } int S3() { return 3; } }
namespace G { struct S2 { int q; }; struct S3 { int q; }; int S3() { return 1; } }
namespace N { int A() { return 2; } }
using A::f;
int f(int a) { return 2; }
int h(int a) { return 2; }
namespace C { int h() { return 3; } }
using C::h;
using A::g;
int g(int a) { return 2; }
using A::h2;
int h2(int a) { return 2; }
using A::h3;
int h3(int a) { return 2; }
using A::y;
struct y { int q; };
struct z { int q; };
using A::z;
using A::S;
constant int S = 7;
using A::w;
using A::w;
using A::k;
using B::k;
using G::S2;
using B::S2;
using G::S3;
using B::S3;
int withParameter(int k) { k = k + 1; return k; }
namespace n1 { constant int q = 1; }
using A::n1;
namespace n2 { constant int q = 1; }
using A::n2;
namespace M { namespace n1 { constant int q = 1; } using A::n1; }
namespace D { constant int e = 1; }
using D::e;
enum E { e };
using namespace A;
constant int v = 2;
struct T { int f() const { return 1; } };
kernel void namespace_and_using_declaration_non_conflicts(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]])
{
    int A = int(i);
    int k = 3;
    k = 5;
    out[0] = A::x;
    out[2] = k + withParameter(1);
    out[1] = A + A::x2 + N::A() + w + v + int(E::e);
}
fragment float4 namespace_and_using_declaration_non_conflicts_entry(float4 k [[position]])
{ return k; }
