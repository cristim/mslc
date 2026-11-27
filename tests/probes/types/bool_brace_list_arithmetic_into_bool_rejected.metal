// EXPECT: error cannot be narrowed to type 'bool'
//
// Issue #160: Apple rejects true + true (2) in a bool brace list as a narrowing; so does mslc.
kernel void bool_brace_list_arithmetic_into_bool_rejected(device int *o [[buffer(0)]])
{
    bool f{true + true};
    o[0] = f;
}
