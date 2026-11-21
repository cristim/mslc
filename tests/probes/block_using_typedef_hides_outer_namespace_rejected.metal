// EXPECT: error "K" in "K::y" is not a namespace the file declares
//
// Apple: "is not a class, namespace, or enumeration": the typedef brought in by the using declaration hides namespace K.
#include <metal_stdlib>
using namespace metal;
namespace K { constant int y = 1; }
namespace N { typedef int K; }
int g(int x) { using N::K; return K::y; }
kernel void block_using_typedef_hides_outer_namespace_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = g(int(i)); }
