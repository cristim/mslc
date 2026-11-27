// EXPECT: valid
// RUN: grid=4 local=4 cases=3
//
// A struct member of a buffer struct converts between the buffer's laid-out form and a local's value
// form whichever way the copy goes: the member alone, an element of an array of it, the whole
// array, and the enclosing struct with its nested members. Each store is read back through the
// buffer, so a member converted in the wrong layout shows in the values.
#include <metal_stdlib>

struct Light
{
    uint tag;
    uint3 direction;
};

struct Lights
{
    uint first;
    Light one;
    Light pair[2];
    metal::array<Light, 2> both;
};

kernel void struct_member_nested_buffer_copy(device uint *out [[buffer(0)]],
    device Lights *p [[buffer(1)]], uint i [[thread_position_in_grid]])
{
    device Lights &s = p[i];
    Light l = s.pair[1];
    metal::array<Light, 2> a = s.both;
    Lights whole = s;
    Light m;
    m.tag = whole.one.tag + 7;
    m.direction = l.direction + a[0].direction;
    s.pair[0] = s.one;
    s.both[1] = m;
    s.one.direction.y = whole.both[1].tag;
    out[i] = s.pair[0].direction.x + s.both[1].tag + s.both[1].direction.z + s.one.direction.y + a[1].tag;
}
