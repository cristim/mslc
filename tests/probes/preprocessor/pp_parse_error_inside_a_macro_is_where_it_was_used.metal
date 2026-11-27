// EXPECT: error pp_parse_error_inside_a_macro_is_where_it_was_used.metal:4:1: unexpected
// Tokens a macro produced are placed at the use of the macro.
#define BAD int stray;
BAD
