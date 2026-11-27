// EXPECT: valid
//
// "restrict" as a local variable, and as a for-loop variable.
kernel void restrict_local_variable_is_valid(device float* o [[buffer(0)]])
{
	float restrict = 1.0;
	for (int i = 0; i < 2; i++) { restrict += 1.0; }
	o[0] = restrict;
}
