// EXPECT: valid
// Groups are numbered across CAPTURE lines, so $10 exists; replacing $1 must not
// rewrite the front of it. $1..$9 spell OpMemoryM, $10 is the OpReturn of the next line.
// DISASM-CAPTURE: (O)(p)(M)(e)(m)(o)(r)(y)(M)odel 
// DISASM-CAPTURE: (OpReturn)
// DISASM-MATCH-CAPTURED: $1$2$3$4$5$6$7$8$9odel .*$10[^A-Za-z]*OpFunctionEnd
kernel void k() {}
