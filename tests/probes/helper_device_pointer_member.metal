// EXPECT: valid
// p->field and p[i].field through a helper's device pointer parameter, with the
// struct member read and written through it.
#include <metal_stdlib>
using namespace metal;
struct Pair { float a; float b; };
float readA(device Pair* p, uint i) { return p[i].b; }
void writeB(device Pair* p, uint i, float v) { p->b = p->b + v; }
kernel void helper_device_pointer_member(device Pair* out [[buffer(0)]], device Pair* in [[buffer(1)]],
                                         uint i [[thread_position_in_grid]])
{
    writeB(out, i, 1.0f);
    out[i].a = readA(in, i);
}
