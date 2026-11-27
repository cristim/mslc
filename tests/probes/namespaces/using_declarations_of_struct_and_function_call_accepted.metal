// EXPECT: valid
// Regression (cross-agent review of #264): "using A::S; using B::S;" where A::S is a struct
// and B::S is a function is already accepted at the declaration (a tag beside an object), but
// calling S() wrongly reported "ambiguous: it names A::S and B::S" because collectOwn inserted
// both targets into the lookup. A function or variable a using declaration brings in hides a
// tag it brings in alongside it, the same way an own-declared name does. Apple's IR calls B::S
// and stores 2; so does master.
#include <metal_stdlib>
using namespace metal;
namespace A { struct S { int q; }; }
namespace B { int S() { return 2; } }
using A::S;
using B::S;
kernel void k(device int* out [[buffer(0)]]) {
	out[0] = S();
}
