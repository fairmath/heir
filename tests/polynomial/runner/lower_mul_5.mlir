// WARNING: this file is autogenerated. Do not edit manually, instead see
// tests/polynomial/runner/generate_test_cases.py

//-------------------------------------------------------
// entry and check_prefix are re-set per test execution
// DEFINE: %{entry} =
// DEFINE: %{check_prefix} =

// DEFINE: %{compile} = heir-opt %s --heir-polynomial-to-llvm
// DEFINE: %{run} = mlir-cpu-runner -e %{entry} -entry-point-result=void --shared-libs="%mlir_lib_dir/libmlir_c_runner_utils%shlibext,%mlir_runner_utils"
// DEFINE: %{check} = FileCheck %s --check-prefix=%{check_prefix}
//-------------------------------------------------------

func.func private @printMemrefI32(memref<*xi32>) attributes { llvm.emit_c_interface }

// REDEFINE: %{entry} = test_5
// REDEFINE: %{check_prefix} = CHECK_TEST_5
// RUN: %{compile} | %{run} | %{check}

#ideal_5 = #polynomial.int_polynomial<1 + x**12>
#ring_5 = #polynomial.ring<coefficientType = i32, coefficientModulus=16 : i32, polynomialModulus=#ideal_5>
!poly_ty_5 = !polynomial.polynomial<ring=#ring_5>

func.func @test_5() {
  %const0 = arith.constant 0 : index
  %0 = polynomial.constant int<1 + x**2> : !poly_ty_5
  %1 = polynomial.constant int<1 + x**3> : !poly_ty_5
  %2 = polynomial.mul %0, %1 : !poly_ty_5


  %tensor = polynomial.to_tensor %2 : !poly_ty_5 -> tensor<12xi32>

  %ref = bufferization.to_memref %tensor : memref<12xi32>
  %U = memref.cast %ref : memref<12xi32> to memref<*xi32>
  func.call @printMemrefI32(%U) : (memref<*xi32>) -> ()
  return
}
// expected_result: Poly(x**5 + x**3 + x**2 + 1, x, domain='ZZ[16]')
// CHECK_TEST_5: {{(1|-15)}}, 0, {{(1|-15)}}, {{(1|-15)}}, 0, {{(1|-15)}}
