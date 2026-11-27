// EXPECT: error "A::K" in "A::K::y" is not a namespace the file declares
// A's own typedef K is its own answer to "K", even though it names no namespace, so the
// walk must not search A's directive (to M, whose namespace K has a "y") past it either;
// "A::K::y" is rejected the same way master already rejects it (Apple: "'A::K' (aka
// 'int') is not a class, namespace, or enumeration"; mslc's wording is pre-existing,
// unchanged here).
#include <metal_stdlib>
using namespace metal;
namespace M { namespace K { constant int y = 7; } }
namespace A { using namespace M; typedef int K; }
kernel void qualified_namespace_own_typedef_rejected(device int* out [[buffer(0)]])
{ out[0] = int(A::K::y); }
