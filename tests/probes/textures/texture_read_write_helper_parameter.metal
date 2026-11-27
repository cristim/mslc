// EXPECT: valid
// A helper takes a read_write texture as the pointer to its variable, like the other accesses.
// DISASM: OpFunctionCall
// DISASM-MATCH: OpImageRead %v4float
// DISASM: OpImageWrite
#include <metal_stdlib>
using namespace metal;
void bump(texture2d<float, access::read_write> t, uint2 p) { t.write(t.read(p) + 1.0, p); }
kernel void hit(texture2d<float, access::read_write> a [[texture(0)]], uint2 g [[thread_position_in_grid]]) {
  bump(a, g);
}
