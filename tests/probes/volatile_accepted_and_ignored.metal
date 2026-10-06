// EXPECT: valid
//
// volatile is accepted on a local and on a pointee and is ignored: it is not
// lowered to a Volatile memory operand (pre-existing; this probe pins only that
// it is not rejected).
kernel void volatile_accepted_and_ignored(device float volatile* p [[buffer(0)]])
{
	volatile float x = 1.0;
	x = 2.0;
	p[0] = x;
}
