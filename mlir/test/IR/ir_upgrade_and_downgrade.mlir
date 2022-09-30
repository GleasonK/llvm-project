// RUN: OP_VERSION=38 mlir-opt %s --mlir-print-with-downgrades | FileCheck %s

// Version 38: No attribute
// Version 39: Added attr version_39_attr
// Version 40: Rename attr version_39_attr -> version_40_attr
// Forward compatibility: Downgrade to v39.

// CHECK-LABEL: func @ir_gets_upgraded()
func.func @ir_gets_upgraded() -> index {
  // %0 = "test.test_upgrade"() {version_39_attr = 1 : i32} : () -> index
  %1 = "test.test_upgrade"() : () -> index
  return %1 : index
}
