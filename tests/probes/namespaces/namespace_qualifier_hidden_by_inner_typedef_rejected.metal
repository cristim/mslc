// EXPECT: error "K" in "K::y" is not a namespace the file declares
//
// Apple: "'N::K' (aka 'int') is not a class, namespace, or enumeration". The typedef hides the outer namespace K.
#include <metal_stdlib>
using namespace metal;
namespace K { constant int y = 1; }
namespace N { typedef int K; constant int z = K::y; }
kernel void namespace_qualifier_hidden_by_inner_typedef_rejected(device int* o [[buffer(0)]], uint i [[thread_position_in_grid]])
{ o[i] = N::z; }
