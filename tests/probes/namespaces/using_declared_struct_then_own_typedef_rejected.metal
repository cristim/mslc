// EXPECT: error "T" conflicts with the using declaration of "B::T" in the same namespace
// Regression (cross-agent review of #264): the mirror of using_declared_typedef_then_own_
// struct_rejected, with the kinds swapped. A using declaration of a struct comes first, so a
// later plain typedef of the same alias stays rejected.
#include <metal_stdlib>
using namespace metal;
namespace B { struct T { int q; }; }
using B::T;
typedef int T;
