// RUN: heir-opt -forward-insert-to-extract %s | FileCheck %s


#encoding = #lwe.polynomial_evaluation_encoding<cleartext_start = 32, cleartext_bitwidth = 32>
#my_poly = #polynomial.int_polynomial<1 + x**16>
#ring= #polynomial.ring<coefficientType = i32, coefficientModulus = 463187969 : i32, polynomialModulus=#my_poly>
#rlwe_params = #lwe.rlwe_params<ring=#ring>


!cc = !openfhe.crypto_context
!pt = !lwe.rlwe_plaintext<encoding = #encoding, ring=#ring, underlying_type=f32>
!ptf16 = !lwe.rlwe_plaintext<encoding = #encoding, ring=#ring, underlying_type=f16>
!ct = !lwe.rlwe_ciphertext<encoding = #encoding, rlwe_params = #rlwe_params, underlying_type=f32>


//  CHECK-LABEL: @successful_forwarding
//  CHECK-SAME:  (%[[ARG0:.*]]: !openfhe.crypto_context,


func.func @successful_forwarding(%arg0: !cc, %arg1: tensor<1x16x!ct>, %arg2: tensor<1x16x!ct>, %arg3: tensor<16xf64>, %arg4: tensor<16xf64>) -> tensor<1x16x!ct> {

  // CHECK-NEXT: %[[C0:.*]] = arith.constant 0 : index
  %c0 = arith.constant 0 : index
  // CHECK-NEXT: %[[C1:.*]] = arith.constant 1 : index
  %c1 = arith.constant 1 : index

  //  CHECK-NEXT: %[[EXTRACTED:.*]] = tensor.extract
  %extracted = tensor.extract %arg1[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[EXTRACTED0:.*]] = tensor.extract
  %extracted_0 = tensor.extract %arg2[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[VAL0:.*]] = openfhe.make_ckks_packed_plaintext %[[ARG0]]
  %0 = openfhe.make_ckks_packed_plaintext %arg0, %arg3 : (!cc, tensor<16xf64>) -> !pt
  //  CHECK-NEXT: %[[VAL1:.*]] = openfhe.mul_plain %[[ARG0]], %[[EXTRACTED]], %[[VAL0]]
  %1 = openfhe.mul_plain %arg0, %extracted, %0 : (!cc, !ct, !pt) -> !ct
  //  CHECK-NEXT: %[[VAL2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL1]]
  %2 = openfhe.add %arg0, %extracted_0, %1 : (!cc, !ct, !ct) -> !ct

  //  CHECK-NEXT: %[[INSERTED0:.*]] = tensor.insert %[[VAL2]]
  %inserted = tensor.insert %2 into %arg2[%c0, %c0] : tensor<1x16x!ct>

  //  CHECK-NEXT: %[[EXTRACTED1:.*]] = tensor.extract
  %extracted_1 = tensor.extract %arg1[%c0, %c1] : tensor<1x16x!ct>
  //  CHECK-NOT: tensor.extract %[[INSERTED0]]
  %extracted_2 = tensor.extract %inserted[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[VAL3:.*]] = openfhe.make_ckks_packed_plaintext
  %3 = openfhe.make_ckks_packed_plaintext %arg0, %arg4 : (!cc, tensor<16xf64>) -> !lwe.rlwe_plaintext<encoding = #encoding, ring = <coefficientType = i32, coefficientModulus = 463187969 : i32, polynomialModulus = <1 + x**16>>, underlying_type = f32>
  //  CHECK-NEXT: %[[VAL4:.*]] = openfhe.mul_plain
  %4 = openfhe.mul_plain %arg0, %extracted_1, %3 : (!cc, !ct, !lwe.rlwe_plaintext<encoding = #encoding, ring = <coefficientType = i32, coefficientModulus = 463187969 : i32, polynomialModulus = <1 + x**16>>, underlying_type = f32>) -> !ct
  //  CHECK-NEXT: %[[VAL5:.*]] = openfhe.add
  %5 = openfhe.add %arg0, %extracted_2, %4 : (!cc, !ct, !ct) -> !ct
  //  CHECK-NEXT: %[[INSERTED1:.*]] = tensor.insert
  %inserted_3 = tensor.insert %5 into %inserted[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: return %[[INSERTED1]]
  return %inserted_3 : tensor<1x16x!ct>
}


//hits def == nullptr
//  CHECK-LABEL: @forward_from_func_arg
//  CHECK-SAME:  (%[[ARG0:.*]]: !openfhe.crypto_context,

func.func @forward_from_func_arg(%arg0: !cc, %arg1: tensor<1x16x!ct>, %arg2: tensor<1x16x!ct>)-> !ct {
  // CHECK-NEXT: %[[C0:.*]] = arith.constant 0 : index
  %c0 = arith.constant 0 : index
  //  CHECK-NEXT: %[[EXTRACTED:.*]] = tensor.extract
  %extracted = tensor.extract %arg1[%c0, %c0] : tensor<1x16x!ct>

  return %extracted : !ct
}

//  CHECK-LABEL: @forwarding_with_an_insert_in_between
//  CHECK-SAME:  (%[[ARG0:.*]]: !openfhe.crypto_context,

func.func @forwarding_with_an_insert_in_between(%arg0: !cc, %arg1: tensor<1x16x!ct>, %arg2: tensor<1x16x!ct>, %arg3: tensor<16xf64> )-> !ct {

  // CHECK-NEXT: %[[C0:.*]] = arith.constant 0 : index
  %c0 = arith.constant 0 : index

  //  CHECK-NEXT: %[[EXTRACTED:.*]] = tensor.extract
  %extracted = tensor.extract %arg1[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[EXTRACTED0:.*]] = tensor.extract
  %extracted_0 = tensor.extract %arg2[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[VAL0:.*]] = openfhe.make_ckks_packed_plaintext %[[ARG0]]
  %0 = openfhe.make_ckks_packed_plaintext %arg0, %arg3 : (!cc, tensor<16xf64>) -> !pt
  //  CHECK-NEXT: %[[VAL1:.*]] = openfhe.mul_plain %[[ARG0]], %[[EXTRACTED]], %[[VAL0]]
  %1 = openfhe.mul_plain %arg0, %extracted, %0 : (!cc, !ct, !pt) -> !ct
  //  CHECK-NEXT: %[[VAL2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL1]]
  %2 = openfhe.add %arg0, %extracted_0, %1 : (!cc, !ct, !ct) -> !ct
  //  CHECK-NEXT: %[[VALA2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL2]]
  %a2 = openfhe.add %arg0, %extracted_0, %2 : (!cc, !ct, !ct) -> !ct
  //  CHECK-NOT: tensor.insert %[[VAL2]]
  %inserted = tensor.insert %2 into %arg2[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NOT: tensor.insert %[[VALA2]]
  %inserted_1 = tensor.insert %a2 into %arg1[%c0, %c0] : tensor<1x16x!ct>

  //  CHECK-NOT: tensor.extract
  %extracted_2 = tensor.extract %inserted_1[%c0, %c0] : tensor<1x16x!ct>
  // CHECK: return %[[VALA2]]
  return %extracted_2 : !ct
}

//  CHECK-LABEL: @forwarding_with_an_operation_in_between
//  CHECK-SAME:  (%[[ARG0:.*]]: !openfhe.crypto_context,

func.func @forwarding_with_an_operation_in_between(%arg0: !cc, %arg1: tensor<1x16x!ct>, %arg2: tensor<1x16x!ct>, %arg3: tensor<16xf64>, %arg4: i1 )-> !ct {

  // CHECK-NEXT: %[[C0:.*]] = arith.constant 0 : index
  %c0 = arith.constant 0 : index

  //  CHECK-NEXT: %[[EXTRACTED:.*]] = tensor.extract
  %extracted = tensor.extract %arg1[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[EXTRACTED0:.*]] = tensor.extract
  %extracted_0 = tensor.extract %arg2[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[VAL0:.*]] = openfhe.make_ckks_packed_plaintext %[[ARG0]]
  %0 = openfhe.make_ckks_packed_plaintext %arg0, %arg3 : (!cc, tensor<16xf64>) -> !pt
  //  CHECK-NEXT: %[[VAL1:.*]] = openfhe.mul_plain %[[ARG0]], %[[EXTRACTED]], %[[VAL0]]
  %1 = openfhe.mul_plain %arg0, %extracted, %0 : (!cc, !ct, !pt) -> !ct
  //  CHECK-NEXT: %[[VAL2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL1]]
  %2 = openfhe.add %arg0, %extracted_0, %1 : (!cc, !ct, !ct) -> !ct

  //  CHECK-NOT: %[[INSERTED0:.*]] = tensor.insert %[[VAL2]]
  %inserted = tensor.insert %2 into %arg2[%c0, %c0] : tensor<1x16x!ct>

  scf.if %arg4 {
    //  CHECK-NOT: %[[VALa2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL2]]
    %a2 = openfhe.add %arg0, %extracted_0, %2 : (!cc, !ct, !ct) -> !ct
    //  CHECK-NOT: tensor.insert %[[VAL1]]
    %inserted_1 = tensor.insert %a2 into %arg2[%c0, %c0] : tensor<1x16x!ct>
  }
    //  CHECK-NOT: tensor.extract
  %extracted_2 = tensor.extract %inserted[%c0, %c0] : tensor<1x16x!ct>
  return %extracted_2 : !ct
}


//  CHECK-LABEL: @two_extracts_both_forwarded
//  CHECK-SAME:  (%[[ARG0:.*]]: !openfhe.crypto_context,

func.func @two_extracts_both_forwarded(%arg0: !cc, %arg1: tensor<1x16x!ct>, %arg2: tensor<1x16x!ct>, %arg3: tensor<16xf64>) -> !ct {

  // CHECK-NEXT: %[[C0:.*]] = arith.constant 0 : index
  %c0 = arith.constant 0 : index

  //  CHECK-NEXT: %[[EXTRACTED:.*]] = tensor.extract
  %extracted = tensor.extract %arg1[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[EXTRACTED0:.*]] = tensor.extract
  %extracted_0 = tensor.extract %arg2[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NEXT: %[[VAL0:.*]] = openfhe.make_ckks_packed_plaintext %[[ARG0]]
  %0 = openfhe.make_ckks_packed_plaintext %arg0, %arg3 : (!cc, tensor<16xf64>) -> !pt
  //  CHECK-NEXT: %[[VAL1:.*]] = openfhe.mul_plain %[[ARG0]], %[[EXTRACTED]], %[[VAL0]]
  %1 = openfhe.mul_plain %arg0, %extracted, %0 : (!cc, !ct, !pt) -> !ct
  //  CHECK-NEXT: %[[VAL2:.*]] = openfhe.add %[[ARG0]], %[[EXTRACTED0]], %[[VAL1]]
  %2 = openfhe.add %arg0, %extracted_0, %1 : (!cc, !ct, !ct) -> !ct

  %inserted = tensor.insert %2 into %arg2[%c0, %c0] : tensor<1x16x!ct>

  //  CHECK-NOT: tensor.extract
  %extracted_1 = tensor.extract %inserted[%c0, %c0] : tensor<1x16x!ct>
  //  CHECK-NOT: tensor.extract
  %extracted_2 = tensor.extract %inserted[%c0, %c0] : tensor<1x16x!ct>
  // CHECK: openfhe.add %[[ARG0]], %[[VAL2]], %[[VAL2]]
  %3 = openfhe.add %arg0, %extracted_1, %extracted_2 : (!cc, !ct, !ct) -> !ct
  return %3: !ct
}
