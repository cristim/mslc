// EXPECT: valid
// An arrayed storage image takes its layer as the last coordinate component.
// DISASM-MATCH: OpTypeImage %float 2D 0 1 0 2 Unknown
// DISASM: OpCompositeConstruct %v3uint
// DISASM: OpImageWrite
// DISASM-MATCH: OpImageRead %v4float
// DISASM: OpImageQuerySize %v3uint
// DISASM-NOT: OpImageQuerySizeLod
#include <metal_stdlib>
using namespace metal;
kernel void hit(texture2d_array<float, access::read_write> a [[texture(0)]], device uint* out [[buffer(0)]], uint2 t [[thread_position_in_grid]]) {
  a.write(a.read(t, 1u), t, 2u);
  out[0] = a.get_array_size() + a.get_width() + a.get_height();
}
