// EXPECT: error "::metal::mem_flags" is not a namespace
#include <metal_stdlib>
namespace M = ::metal::mem_flags;
kernel void k() {
}
