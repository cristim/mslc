// EXPECT: valid
//
// The initialiser of a derived constant is file-scope code, wherever the constant is first read: the local x and
// the loop's kC of the kernel do not hide the function constant x and the constant kC from it. On an RTX 3060 with
// x = 100 the kernel writes 103.
constant int x [[function_constant(0)]];
constant int kC = 3;
constant int y = x + kC;
kernel void k(device int* out [[buffer(0)]]) {
	int x = 5;
	for (int kC = 0; kC < 1; ++kC) { out[kC] = y + x * 0; }
}
