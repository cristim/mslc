// EXPECT: valid
//
// Issue #160: a live expression statement with no effect compiles (Apple only warns -Wunused-value).
kernel void live_code_unused_expression_statements_valid(device int *o [[buffer(0)]])
{
    int x = o[0];
    x + 1;
    x;
    (void)x;
    o[0] = x;
}
