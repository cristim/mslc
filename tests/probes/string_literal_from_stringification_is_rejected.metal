// EXPECT: error a string literal is not lowered
//
// The stringification operator is the one place a string is produced inside
// the compiler rather than read from the source, and it runs through the same
// isStringLiteral helper that the quoted #include and #line paths use. Giving
// the literal its own token kind touches all three, so this pins the one that
// synthesises: the expansion of #x is a string, and it is refused where the
// parser reads it, not by the stringifier.
//
// The macro is otherwise unused, so nothing about the diagnostic depends on
// where the literal is expanded.
#define STR(x) #x
#define PHRASE STR(abc)
kernel void string_literal_from_stringification_is_rejected()
{
    constant char *s = PHRASE;
    s = nullptr;
}