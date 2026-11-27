// EXPECT: error a string literal is not lowered
//
// The lexer returned a string literal as an Identifier token, so a string in
// expression position was read as a name and the diagnostic described a failed
// lookup for an identifier spelled with quotes: "\"hello\" is not a parameter,
// local, constant or builtin mslc knows about". Nothing there says mslc has no
// string type, which is the actual reason.
//
// Apple rejects this source too, with a type diagnostic, so mslc is not
// over-rejecting; only the message named the wrong thing. The probe pins the
// token kind through the message, since a disassembly needle cannot see a
// diagnostic.
kernel void string_literal_in_expression_rejected()
{
    int x = "hello";
    x = 1;
}