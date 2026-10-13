// EXPECT: error "T" conflicts with the using declaration of "A::T" in the same namespace
// Regression (cross-agent review of #264): the widened tag exemption (struct_then_using_
// declared_typedef_of_other_namespace_accepted) only applies when the using declaration is
// the later one. Here the using declaration of a typedef comes first, so a later plain struct
// of the same alias stays rejected, as Apple reports it (rejectUsingConflict keeps the narrow,
// function/variable-only exemption for this order).
#include <metal_stdlib>
using namespace metal;
namespace A { typedef int T; }
using A::T;
struct T { int q; };
