// EXPECT: error "A::K" is declared and cannot also be a namespace alias
#include <metal_stdlib>
using namespace metal;
namespace M { constant int y = 7; }
namespace D { typedef int K; }
namespace A { using D::K; namespace K = M; }
kernel void namespace_alias_typedef_after_using_type_rejected(device int* out [[buffer(0)]])
{ out[0] = int(A::K::y); }
