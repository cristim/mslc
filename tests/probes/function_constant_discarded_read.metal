// EXPECT: valid
// REFLECT-NOT: "name": "h"
//
// A function constant read only in an expression that is emitted to learn its type and then discarded
// (here a case label, which takes the constant's value of the conditional) is not listed.
#include <metal_stdlib>
using namespace metal;
constant half h [[function_constant(0)]];
kernel void k(device int* out [[buffer(0)]], uint i [[thread_position_in_grid]]) {
    switch (int(i)) {
        case (true ? 0 : int(h)): out[0] = 1; break;
        default: out[0] = 2; break;
    }
}
