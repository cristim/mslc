// EXPECT: error "N::metal" is not a namespace
#include <metal_stdlib>
namespace N { }
namespace M = N::metal;
kernel void k() { }
