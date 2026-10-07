// EXPECT: error a string literal is not lowered
//
// A string reaching the parser through a macro, so the rejection is not
// specific to text written inline. A macro body holding a string is legal and
// Apple expands it; what mslc refuses is the expanded literal in expression
// position, and it must refuse it after expansion, not before.
#define GREETING "hello"
kernel void string_literal_from_a_macro_is_rejected()
{
    int x = GREETING;
    x = 1;
}