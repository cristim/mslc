// EXPECT: valid
//
// Issue #160: expression statements with no effect (and a void cast) after a return are accepted by
// Apple, in the function body, an if body, a loop body and a switch case.
#include <metal_stdlib>
using namespace metal;
struct S { int m; };
kernel void dead_code_expression_statements_valid(device int *o [[buffer(0)]], device S *s [[buffer(1)]])
{
    o[0] = 1;
    return;
    int x = 1;
    x + 1;
    x;
    (void)x;
    x++;
    x += 2;
    s->m = 4;
    min(x, 2);
    if (x) { x * 2; o[1] = 1; }
    for (int i = 0; i < 2; i++) { x; }
    switch (x) { case 1: x - 1; break; default: break; }
    x, x;
}
