// EXPECT: error a string literal is not lowered
//
// The same rejection one level down: a string in an argument list. It used to
// reach the name lookup with the call's arguments still to be read, so the
// diagnostic named the call and not the literal.
int twice(int v) { return v * 2; }
kernel void string_literal_as_a_function_argument_rejected()
{
    int x = twice("abc");
    x = 1;
}