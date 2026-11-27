// EXPECT: valid
// REFLECT: "metal_index": 5
// REFLECT: "metal_index": 3
//
// A declared name sizes an attribute index the same way it sizes an array length, through a
// cast to an int type (Apple rejects a bare ulong there): 20 / 4 is 5 and the 12-byte row of a
// float[2][3] over a float is 3. Apple's compiler reads the same indices.
#include <metal_stdlib>
using namespace metal;
constant float G[5] = {1, 2, 3, 4, 5};
constant float GM[2][3] = {{1, 2, 3}, {4, 5, 6}};
kernel void sizeof_name_attribute_index(device float *o [[buffer((uint)sizeof(G) / 4)]],
    device float *p [[buffer(int(sizeof(GM[0]) / sizeof(GM[0][0])))]])
{
    o[0] = 1;
    p[0] = 1;
}
