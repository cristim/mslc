// EXPECT: error in an expression
// Apple: "expected expression". A trailing comma in a for increment has no operand.
kernel void for_comma_missing_operand_rejected(device int *o [[buffer(0)]]) {
  for (int j = 0; j < 2; j++, ) {
    o[j] = j;
  }
}
