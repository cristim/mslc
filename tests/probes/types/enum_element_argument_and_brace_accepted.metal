// EXPECT: valid
// The still-accepted side of #259's checks: an enum store/read through a buffer
// element, a correct argument and brace initializer in every form the issue
// touches, a correct return value in every form Task 5 checks, and an
// anonymous enum's enumerator passing an argument and a brace leaf of its own
// typedef'd type. mslc can not tell an untagged enumerator from the target
// enum's own, so it lets "<anonymous>" through everywhere; that includes into
// an unrelated enum like Mode, a known over-accept this probe deliberately
// does not pin.
enum Mode { A, B, X };
struct S { Mode m; int n; };
typedef enum { TA, TB } Anon;

void mk(Mode m) { }
int use(Mode m) { return (int)m; }
void ma(Anon a) { }

Mode retLiteral() { return A; }
Mode retConditional(bool c, Mode m) { return c ? m : A; }
Mode retCast() { return (Mode)1; }

kernel void enum_element_argument_and_brace_accepted(device Mode *bm [[buffer(0)]],
	device int *o [[buffer(1)]], uint u [[thread_position_in_grid]]) {
	bool c = u > 0;
	Mode m = A;

	// c1
	m = c ? A : X;

	// p4-p8
	bm[0] = A;
	*bm = A;
	Mode p1 = bm[0];
	Mode p2 = *bm;
	m = *bm;

	// a3, a5, mk(bm[0])
	mk(A);
	int used = use(m);
	mk(bm[0]);

	// b2, b5, b13, designated, S{A, 1}, array
	Mode b2 = {A};
	S b5 = {A, 1};
	Mode b13 = {};
	S designated = { .m = A, .n = 1 };
	S listed = S{A, 1};
	Mode arr[2] = {A, B};

	// r1 accept side
	Mode r1 = retLiteral();
	Mode r2 = retConditional(c, m);
	Mode r3 = retCast();

	// the anonymous enumerator passes an argument and a brace leaf of its own
	// (typedef'd) Anon type, via the same "<anonymous>" pass-through that also
	// lets it cross into Mode (deliberately not exercised here; see the file
	// header).
	ma(TB);
	Anon anonBrace = {TB};

	o[0] = used + (int)p1 + (int)p2 + (int)b2 + (int)b13 + (int)arr[0] + (int)r1 + (int)r2 + (int)r3 + (int)anonBrace;
}
