// EXPECT: valid
// DISASM-CAPTURE: (%[0-9]+) = OpLoad %uint (%[0-9]+)[^%A-Za-z]*(%[0-9]+) = OpAtomicCompareExchange %uint %[0-9]+ %uint_1[_0-9]* %uint_0[_0-9]* %uint_0[_0-9]* %uint_9 (%[0-9]+)
// DISASM-MATCH-CAPTURED: $3 = OpAtomicCompareExchange %uint %[0-9]+ %uint_1[_0-9]* %uint_0[_0-9]* %uint_0[_0-9]* %uint_9 $1
// DISASM-MATCH-CAPTURED: OpStore $2 $3[^%A-Za-z]*%[0-9]+ = OpIEqual %bool $3 $1
// DISASM-NO-MATCH: OpINotEqual
// The call's value flow, at device scope (1). $1 is the load of the expected variable $2 and is the
// comparator, the second operand after desired. $3 is the old value the exchange returns. It is stored
// back into that same variable $2, and the bool is OpIEqual of $3 and the comparator $1.
kernel void atomic_compare_exchange_weak_value_flow(device uint *o [[buffer(0)]], device atomic_uint *c [[buffer(1)]]) {
  uint e = 5u;
  o[0] = atomic_compare_exchange_weak_explicit(&c[0], &e, 9u, memory_order_relaxed, memory_order_relaxed) ? 1u : 0u;
  o[1] = e;
}
