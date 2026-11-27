// EXPECT: error cannot be narrowed to 'int' in an initializer list
//
// Apple rejects a float in an int's brace list as a narrowing; the "=" form is narrowing too (#390).
constant int k{1.5};

kernel void file_scope_constant_brace_narrowing_rejected(device int *out [[buffer(0)]])
{
    out[0] = k;
}
