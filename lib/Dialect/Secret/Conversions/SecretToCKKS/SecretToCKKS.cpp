#include "lib/Dialect/Secret/Conversions/SecretToCKKS/SecretToCKKS.h"

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

#include "lib/Dialect/CKKS/IR/CKKSDialect.h"
#include "lib/Dialect/CKKS/IR/CKKSOps.h"
#include "lib/Dialect/LWE/IR/LWEAttributes.h"
#include "lib/Dialect/LWE/IR/LWEDialect.h"
#include "lib/Dialect/LWE/IR/LWEOps.h"
#include "lib/Dialect/LWE/IR/LWETypes.h"
#include "lib/Dialect/Secret/IR/SecretDialect.h"
#include "lib/Dialect/Secret/IR/SecretOps.h"
#include "lib/Dialect/Secret/IR/SecretTypes.h"
#include "lib/Dialect/TensorExt/IR/TensorExtOps.h"
#include "lib/Utils/ConversionUtils/ConversionUtils.h"
#include "llvm/include/llvm/ADT/STLExtras.h"    // from @llvm-project
#include "llvm/include/llvm/ADT/SmallVector.h"  // from @llvm-project
#include "llvm/include/llvm/ADT/TypeSwitch.h"   // from @llvm-project
#include "llvm/include/llvm/Support/Casting.h"  // from @llvm-project
#include "mlir/include/mlir/Dialect/Affine/IR/AffineOps.h"  // from @llvm-project
#include "mlir/include/mlir/Dialect/Arith/IR/Arith.h"  // from @llvm-project
#include "mlir/include/mlir/Dialect/Polynomial/IR/Polynomial.h"  // from @llvm-project
#include "mlir/include/mlir/Dialect/Polynomial/IR/PolynomialAttributes.h"  // from @llvm-project
#include "mlir/include/mlir/Dialect/Tensor/IR/Tensor.h"  // from @llvm-project
#include "mlir/include/mlir/IR/Attributes.h"             // from @llvm-project
#include "mlir/include/mlir/IR/BuiltinAttributes.h"      // from @llvm-project
#include "mlir/include/mlir/IR/BuiltinTypes.h"           // from @llvm-project
#include "mlir/include/mlir/IR/Diagnostics.h"            // from @llvm-project
#include "mlir/include/mlir/IR/PatternMatch.h"           // from @llvm-project
#include "mlir/include/mlir/IR/TypeUtilities.h"          // from @llvm-project
#include "mlir/include/mlir/IR/Value.h"                  // from @llvm-project
#include "mlir/include/mlir/IR/ValueRange.h"             // from @llvm-project
#include "mlir/include/mlir/IR/Visitors.h"               // from @llvm-project
#include "mlir/include/mlir/Support/LLVM.h"              // from @llvm-project
#include "mlir/include/mlir/Support/LogicalResult.h"     // from @llvm-project
#include "mlir/include/mlir/Transforms/DialectConversion.h"  // from @llvm-project

namespace mlir::heir {

#define GEN_PASS_DEF_SECRETTOCKKS
#include "lib/Dialect/Secret/Conversions/SecretToCKKS/SecretToCKKS.h.inc"

namespace {

// Returns an RLWE ring given the specified number of bits needed and polynomial
// modulus degree.
// TODO(#536): Integrate a general library to compute appropriate prime moduli
// given any number of bits.
FailureOr<::mlir::polynomial::RingAttr> getRlweRing(MLIRContext *ctx,
                                                    int coefficientModBits,
                                                    int polyModDegree) {
  std::vector<::mlir::polynomial::IntMonomial> monomials;
  monomials.emplace_back(1, polyModDegree);
  monomials.emplace_back(1, 0);
  auto result = ::mlir::polynomial::IntPolynomial::fromMonomials(monomials);
  if (failed(result)) return failure();
  ::mlir::polynomial::IntPolynomial xnPlusOne = result.value();
  switch (coefficientModBits) {
    case 29: {
      auto type = IntegerType::get(ctx, 32);
      return ::mlir::polynomial::RingAttr::get(
          type, IntegerAttr::get(type, APInt(32, 463187969)),
          polynomial::IntPolynomialAttr::get(ctx, xnPlusOne));
    }
    default:
      return failure();
  }
}

}  // namespace

class SecretToCKKSTypeConverter : public TypeConverter {
 public:
  SecretToCKKSTypeConverter(MLIRContext *ctx,
                            ::mlir::polynomial::RingAttr rlweRing,
                            bool packTensorInSlots) {
    addConversion([](Type type) { return type; });

    // Convert secret types to LWE ciphertext types.
    addConversion([ctx, this](secret::SecretType type) -> Type {
      Type valueTy = type.getValueType();
      int bitWidth = getElementTypeOrSelf(valueTy).getIntOrFloatBitWidth();
      // TODO(#785): Set a scaling parameter for floating point values.
      auto ciphertext = lwe::RLWECiphertextType::get(
          ctx,
          lwe::PolynomialEvaluationEncodingAttr::get(ctx, bitWidth, bitWidth),
          lwe::RLWEParamsAttr::get(ctx, 2, ring_), valueTy);
      // Return a single ciphertext if inputs are packed into a single
      // ciphertext SIMD slot or the secret value type is a scalar.
      if (this->packTensorInSlots_ || !isa<TensorType>(valueTy)) {
        return ciphertext;
      }
      // If the input IR does not contain aligned ciphertexts, we will not
      // pack tensors into ciphertext SIMD slots, so tensors are converted
      // into tensors of RLWE ciphertexts.
      assert(dyn_cast<RankedTensorType>(valueTy) &&
             "expected ranked tensor type");
      ciphertext = lwe::RLWECiphertextType::get(
          ctx, ciphertext.getEncoding(), ciphertext.getRlweParams(),
          cast<RankedTensorType>(valueTy).getElementType());
      return RankedTensorType::get(cast<RankedTensorType>(valueTy).getShape(),
                                   ciphertext);
    });

    ring_ = rlweRing;
    packTensorInSlots_ = packTensorInSlots;
  }

 private:
  ::mlir::polynomial::RingAttr ring_;
  bool packTensorInSlots_;
};

class SecretGenericTensorExtractConversion
    : public SecretGenericOpConversion<tensor::ExtractOp, ckks::ExtractOp> {
 public:
  using SecretGenericOpConversion<tensor::ExtractOp,
                                  ckks::ExtractOp>::SecretGenericOpConversion;

  LogicalResult matchAndRewriteInner(
      secret::GenericOp op, TypeRange outputTypes, ValueRange inputs,
      ArrayRef<NamedAttribute> attributes,
      ConversionPatternRewriter &rewriter) const override {
    if (!isa<lwe::RLWECiphertextType>(
            getElementTypeOrSelf(inputs[0].getType()))) {
      return failure();
    }
    if (isa<RankedTensorType>(inputs[0].getType())) {
      // Extracts an element out of a tensor (the secret tensor is not packed).
      rewriter.replaceOpWithNewOp<tensor::ExtractOp>(op, outputTypes, inputs);
      return success();
    }
    // Extracts an element out of a slot of a single ciphertext.
    rewriter.replaceOpWithNewOp<ckks::ExtractOp>(op, outputTypes, inputs);
    return success();
  }
};

class SecretGenericTensorInsertConversion
    : public SecretGenericOpConversion<tensor::InsertOp, tensor::InsertOp> {
 public:
  using SecretGenericOpConversion<tensor::InsertOp,
                                  tensor::InsertOp>::SecretGenericOpConversion;

  LogicalResult matchAndRewriteInner(
      secret::GenericOp op, TypeRange outputTypes, ValueRange inputs,
      ArrayRef<NamedAttribute> attributes,
      ConversionPatternRewriter &rewriter) const override {
    if (!isa<lwe::RLWECiphertextType>(
            getElementTypeOrSelf(inputs[0].getType()))) {
      op.emitError() << "expected secret tensor to be of type RLWE ciphertext"
                     << inputs[0].getType();
      return failure();
    }
    if (isa<RankedTensorType>(inputs[1].getType())) {
      // Insert an element into a tensor (the secret tensor is not packed).
      rewriter.replaceOpWithNewOp<tensor::InsertOp>(op, outputTypes, inputs);
      return success();
    }
    // We can also support the case where the secret tensor is packed into a
    // single ciphertext by converting the insert operation into a zero-hot
    // multiplication followed by an addition of the scalar encoded into a
    // plaintext in the correct slot.
    return failure();
  }
};

struct SecretToCKKS : public impl::SecretToCKKSBase<SecretToCKKS> {
  using SecretToCKKSBase::SecretToCKKSBase;

  void runOnOperation() override {
    MLIRContext *context = &getContext();
    auto *module = getOperation();

    auto rlweRing = getRlweRing(context, coefficientModBits, polyModDegree);
    if (failed(rlweRing)) {
      return signalPassFailure();
    }
    // Ensure that all secret types are uniform and matching the ring
    // parameter size in order to pack tensors into ciphertext SIMD slots.
    bool packTensorInSlots = true;
    WalkResult compatibleTensors = module->walk([&](Operation *op) {
      for (auto value : op->getOperands()) {
        if (auto secretTy = dyn_cast<secret::SecretType>(value.getType())) {
          auto tensorTy = dyn_cast<RankedTensorType>(secretTy.getValueType());
          if (tensorTy && tensorTy.getShape() !=
                              ArrayRef<int64_t>{rlweRing.value()
                                                    .getPolynomialModulus()
                                                    .getPolynomial()
                                                    .getDegree()}) {
            return WalkResult::interrupt();
          }
        }
      }
      return WalkResult::advance();
    });
    if (compatibleTensors.wasInterrupted()) {
      emitWarning(module->getLoc(),
                  "expected secret types to be tensors with dimension matching "
                  "ring parameter, pass will not pack tensors into ciphertext "
                  "SIMD slots");
      packTensorInSlots = false;
    }

    SecretToCKKSTypeConverter typeConverter(context, rlweRing.value(),
                                            packTensorInSlots);
    RewritePatternSet patterns(context);
    ConversionTarget target(*context);
    target.addLegalDialect<ckks::CKKSDialect, lwe::LWEDialect>();
    target.addIllegalDialect<secret::SecretDialect>();
    target.addIllegalOp<secret::GenericOp>();
    addStructuralConversionPatterns(typeConverter, patterns, target);

    // We add an explicit allowlist of operations to mark legal. If we use
    // markUnknownOpDynamicallyLegal, then ConvertAny will be applied to any
    // remaining operations and potentially cause a crash.
    target.addDynamicallyLegalOp<affine::AffineForOp, affine::AffineYieldOp>(
        [&](Operation *op) { return typeConverter.isLegal(op); });

    patterns.add<
        SecretGenericOpCipherConversion<arith::AddIOp, ckks::AddOp>,
        SecretGenericOpCipherConversion<arith::SubIOp, ckks::SubOp>,
        SecretGenericOpCipherConversion<arith::AddFOp, ckks::AddOp>,
        SecretGenericOpCipherConversion<arith::SubFOp, ckks::SubOp>,
        SecretGenericOpCipherConversion<tensor::EmptyOp, tensor::EmptyOp>,
        SecretGenericTensorExtractConversion,
        SecretGenericTensorInsertConversion,
        SecretGenericOpRotateConversion<ckks::RotateOp>,
        SecretGenericOpMulConversion<arith::MulIOp, ckks::MulOp,
                                     ckks::RelinearizeOp>,
        SecretGenericOpMulConversion<arith::MulFOp, ckks::MulOp,
                                     ckks::RelinearizeOp>,
        SecretGenericOpCipherPlainConversion<arith::AddFOp, ckks::AddPlainOp>,
        SecretGenericOpCipherPlainConversion<arith::SubFOp, ckks::SubPlainOp>,
        SecretGenericOpCipherPlainConversion<arith::MulFOp, ckks::MulPlainOp>,
        SecretGenericOpCipherPlainConversion<arith::AddIOp, ckks::AddPlainOp>,
        SecretGenericOpCipherPlainConversion<arith::SubIOp, ckks::SubPlainOp>,
        SecretGenericOpCipherPlainConversion<arith::MulIOp, ckks::MulPlainOp>,
        ConvertAny<affine::AffineForOp>, ConvertAny<affine::AffineYieldOp>>(
        typeConverter, context);

    if (failed(applyPartialConversion(module, target, std::move(patterns)))) {
      return signalPassFailure();
    }
  }
};

}  // namespace mlir::heir
