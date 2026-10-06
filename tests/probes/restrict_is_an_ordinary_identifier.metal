// EXPECT: valid
//
// Bare "restrict" is not an MSL keyword, so it is a name like any other: Apple
// reads it as an identifier in a local, a struct member, a parameter and a
// file-scope constant. "__restrict" is the qualifier.
constant float restrict_k = 1.0;
constant float restrict = 2.0;
struct S { float restrict; };
kernel void restrict_is_an_ordinary_identifier(device float* o [[buffer(0)]], uint restrict [[thread_position_in_grid]])
{
	float restrict2 = restrict_k;
	S s;
	s.restrict = restrict2 + restrict;
	o[0] = s.restrict;
}
