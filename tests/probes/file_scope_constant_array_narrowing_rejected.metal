// EXPECT: error cannot be narrowed to type 'char' in an initializer list
//
// Array elements at file scope are brace-initialised and obey narrowing rules (#390).
constant char a[2]{300, 1};

kernel void file_scope_constant_array_narrowing_rejected(device int *out [[buffer(0)]])
{
    out[0] = int(a[0]);
}
