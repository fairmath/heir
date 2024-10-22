// RUN: heir-opt --canonicalize %s | FileCheck %s

// CHECK-LABEL: @test_sum_rotation_indices
// CHECK: %[[c3:.*]] = arith.constant 3 : i32
// CHECK: tensor_ext.rotate
// CHECK-SAME: %[[c3]]
func.func @test_sum_rotation_indices(%0: tensor<16xi32>) -> tensor<16xi32> {
  %c1 = arith.constant 1 : i32
  %c2 = arith.constant 2 : i32
  %1 = tensor_ext.rotate %0, %c1 : tensor<16xi32>, i32
  %2 = tensor_ext.rotate %1, %c2 : tensor<16xi32>, i32
  return %2 : tensor<16xi32>
}

// CHECK-LABEL: @test_normalize_negative
// CHECK: %[[c3:.*]] = arith.constant 3 : i32
// CHECK: tensor_ext.rotate
// CHECK-SAME: %[[c3]]
func.func @test_normalize_negative(%0: tensor<16xi32>) -> tensor<16xi32> {
  %c1 = arith.constant -13 : i32
  %1 = tensor_ext.rotate %0, %c1 : tensor<16xi32>, i32
  return %1 : tensor<16xi32>
}

// TODO(#924): Right now, we allow multi-dimensional
// tensors to be rotated with a single index because RotateOp verifies that
// there is only one non-unit dimension which is canonically rotated.

// CHECK-LABEL: @test_no_normalize_multidimension
// CHECK: %[[c13:.*]] = arith.constant -13 : i32
// CHECK: tensor_ext.rotate
// CHECK-SAME: %[[c13]]
func.func @test_no_normalize_multidimension(%0: tensor<1x16xi32>) -> tensor<1x16xi32> {
  %c1 = arith.constant -13 : i32
  %1 = tensor_ext.rotate %0, %c1 : tensor<1x16xi32>, i32
  return %1 : tensor<1x16xi32>
}
