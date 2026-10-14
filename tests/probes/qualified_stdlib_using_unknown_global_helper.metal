// EXPECT: error metal::ghost
#include <metal_stdlib>
int ghost(int v){return v+70;}
namespace N {using metal::ghost;}
kernel void k(device int *o [[buffer(0)]]){o[0]=N::ghost(o[1]);}
