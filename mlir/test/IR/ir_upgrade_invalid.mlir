// RUN: OP_VERSION=39 mlir-opt -verify-diagnostics -split-input-file %s

// Version 38: No attribute
// Version 39: Added attr version_39_attr
// Version 40: Rename attr version_39_attr -> version_40_attr
// Attempt to upgrade from v39 -> v40, but attr is missing.

// CHECK-LABEL: func @ir_gets_upgraded()
func.func @ir_gets_upgraded() -> index {
  // expected-error@+1 {{expected version_39_attr for upgrade}}
  %1 = "test.test_upgrade"() : () -> index
  return %1 : index
}
