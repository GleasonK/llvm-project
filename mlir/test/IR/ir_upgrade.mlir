// RUN: OP_VERSION=38 mlir-opt %s | FileCheck %s

// Version 38: No attribute
// Version 39: Added attr version_39_attr
// Version 40: Rename attr version_39_attr -> version_40_attr
// Forward compatibility: None.

// CHECK-LABEL: func @ir_gets_upgraded()
func.func @ir_gets_upgraded() -> index {
  // CHECK: %0 = "test.test_upgrade"() {version_40_attr = 1 : i32} : () -> index
  %0 = "test.test_upgrade"() : () -> index
  return %0 : index
}
