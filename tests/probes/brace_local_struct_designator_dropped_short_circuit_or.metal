// EXPECT: valid
struct S { float a; float b; };
kernel void k(device float* out [[buffer(0)]], constant float* in [[buffer(1)]]) {
	float x = in[0]; float y = in[1];
	S s{.a = (x > 0.0f || y > 0.0f) ? 1.0f : 0.0f, .a = 2.0f};
	if (x > 1.0f || y > 1.0f) { out[1] = s.a; } else { out[2] = s.b; }
	out[0] = s.a;
}
