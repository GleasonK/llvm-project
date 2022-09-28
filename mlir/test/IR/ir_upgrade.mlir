// RUN: mlir-opt %s | FileCheck %s

// CHECK-LABEL: func @ir_gets_upgraded()
func.func @ir_gets_upgraded() -> index {
  %1 = test.test_upgrade 38 : () -> index
  return %1 : index
}
