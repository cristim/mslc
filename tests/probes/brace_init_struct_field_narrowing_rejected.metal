// EXPECT: error constant expression evaluates to 3000000000 which cannot be narrowed to type 'int'

//
// The fields of a braced struct are narrowed the same way.
struct S
{
    int a;
    int b;
};

constant S kValue = { .a = 3000000000, .b = 1 };

kernel void brace_init_struct_field_narrowing_rejected(device uint* out [[buffer(0)]],
                                                       uint i [[thread_position_in_grid]])
{
    out[i] = 0u;
}
