#include "include/Dialect/TensorExt/Transforms/RotateAndReduce.h"

#include "include/Dialect/TensorExt/IR/TensorExtOps.h"
#include "llvm/include/llvm/ADT/DenseSet.h"              // from @llvm-project
#include "llvm/include/llvm/ADT/TypeSwitch.h"            // from @llvm-project
#include "llvm/include/llvm/Support/Debug.h"             // from @llvm-project
#include "mlir/include/mlir/Analysis/SliceAnalysis.h"    // from @llvm-project
#include "mlir/include/mlir/Dialect/Arith/IR/Arith.h"    // from @llvm-project
#include "mlir/include/mlir/Dialect/Tensor/IR/Tensor.h"  // from @llvm-project
#include "mlir/include/mlir/IR/ImplicitLocOpBuilder.h"   // from @llvm-project
#include "mlir/include/mlir/IR/Iterators.h"              // from @llvm-project
#include "mlir/include/mlir/Support/LogicalResult.h"     // from @llvm-project

#define DEBUG_TYPE "rotate-and-reduce"

namespace mlir {
namespace heir {
namespace tensor_ext {

#define GEN_PASS_DEF_ROTATEANDREDUCE
#include "include/Dialect/TensorExt/Transforms/Passes.h.inc"

/// A pass that searches for a length N sequence of binary operations that
/// reduces a length N vector to a single scalar, and replaces it with a
/// logarithmic number of rotations and binary operations.
struct RotateAndReduce : impl::RotateAndReduceBase<RotateAndReduce> {
  using RotateAndReduceBase::RotateAndReduceBase;

  template <typename ArithOp>
  void tryReplace(ArithOp op, DenseSet<Operation *> &visited) {
    LLVM_DEBUG(llvm::dbgs() << "Trying to replace " << *op << "\n");
    SetVector<Operation *> backwardSlice;
    BackwardSliceOptions options;
    // asserts that the parent op has a single region with a single block.
    options.omitBlockArguments = false;

    DenseSet<Value> inputTensors;
    DenseSet<Operation *> visitedReductionOps;
    DenseSet<unsigned> accessIndices;
    DenseMap<llvm::StringRef, int> opCounts;
    opCounts[op->getName().getStringRef()]++;

    // TODO(#523): replace backward slice with a dataflow analysis
    getBackwardSlice(op.getOperation(), &backwardSlice, options);
    for (Operation *upstreamOpPtr : backwardSlice) {
      auto result =
          llvm::TypeSwitch<Operation *, LogicalResult>(upstreamOpPtr)
              .Case<arith::ConstantOp>(
                  [&](auto upstreamOp) { return success(); })
              .template Case<arith::AddIOp, arith::MulIOp>(
                  [&](auto upstreamOp) {
                    opCounts[upstreamOp->getName().getStringRef()]++;
                    // More than one reduction op is mixed in the reduction.
                    if (opCounts.size() > 1) {
                      LLVM_DEBUG(llvm::dbgs()
                                 << "Not replacing op because reduction "
                                    "contains multiple incompatible ops "
                                 << op->getName() << " and "
                                 << upstreamOp->getName() << "\n");
                      return failure();
                    }

                    // TODO(#522): support these non-tensor-extract operands by
                    // saving the values, and applying them again to the final
                    // result.
                    for (Value operand : upstreamOp->getOperands()) {
                      if (operand.getDefiningOp<arith::ConstantOp>()) {
                        LLVM_DEBUG(llvm::dbgs()
                                   << "Not replacing op because reduction "
                                      "includes non-tensor value operands "
                                   << operand << "\n");
                        return failure();
                      }
                    }
                    visitedReductionOps.insert(upstreamOp);
                    return success();
                  })
              .template Case<tensor::ExtractOp>([&](auto tensorOp) {
                inputTensors.insert(tensorOp.getTensor());
                if (inputTensors.size() > 1) {
                  LLVM_DEBUG(
                      llvm::dbgs()
                      << "Not replacing op due to multiple input tensors\n");
                  return failure();
                }

                // If the tensor is not 1D, we can't replace it with a rotate.
                if (tensorOp.getIndices().size() != 1) {
                  LLVM_DEBUG(llvm::dbgs()
                             << "Not replacing op due to >1D input tensor\n");
                  return failure();
                }

                // If the access index is not constant, we can't tell if we are
                // reducing the entire vector (each index occurs exactly once in
                // the redution).
                arith::ConstantOp indexConstant =
                    tensorOp.getIndices()
                        .front()
                        .template getDefiningOp<arith::ConstantOp>();
                if (!indexConstant) {
                  LLVM_DEBUG(
                      llvm::dbgs()
                      << "Not replacing op due to non constant index access;"
                      << " (do you need to run --canonicalize or --sccp?)\n");
                  return failure();
                }
                int64_t accessIndex =
                    indexConstant.getValue().cast<IntegerAttr>().getInt();

                // If the access index was already seen, then fail because some
                // tensor element contributes more than once to the reduction.
                if (accessIndices.count(accessIndex)) {
                  LLVM_DEBUG(
                      llvm::dbgs()
                      << "Not replacing op because input tensor was accessed "
                         "multiple times in at same index\n");
                  return failure();
                }
                LLVM_DEBUG(llvm::dbgs()
                           << "Adding valid index " << accessIndex << "\n");
                accessIndices.insert(accessIndex);
                return success();
              })
              .Default([&](Operation *op) { return failure(); });

      if (failed(result)) {
        return;
      }
    }

    // The test for a match is now: does the number of accessed indices exactly
    // match the size of the tensor? I.e., does each tensor element show up
    // exactly once in the reduction?
    auto tensorShape =
        inputTensors.begin()->getType().cast<RankedTensorType>().getShape();
    if (tensorShape.size() != 1 || tensorShape[0] != accessIndices.size()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Not replacing op because tensor shape ("
                 << inputTensors.begin()->getType()
                 << ") is not fully reduced. Only " << accessIndices.size()
                 << " of " << tensorShape[0] << " indices were accessed\n");
      return;
    }

    // From here we know we will succeed.
    auto b = ImplicitLocOpBuilder(op->getLoc(), op);
    Value inputTensor = *inputTensors.begin();
    Operation *finalOp;
    for (int64_t shiftSize = tensorShape[0] / 2; shiftSize > 0;
         shiftSize /= 2) {
      auto rotatedTensor = b.create<tensor_ext::RotateOp>(
          inputTensor, b.create<arith::ConstantOp>(b.getIndexAttr(shiftSize)));
      auto addOp = b.create<ArithOp>(inputTensor, rotatedTensor);
      finalOp = addOp;
      inputTensor = addOp->getResult(0);
    }

    auto *parentOp = op->getParentOp();
    // We can extract at any index; every index contains the same reduced value.
    auto extractOp = b.create<tensor::ExtractOp>(
        finalOp->getResult(0), b.create<arith::ConstantIndexOp>(0).getResult());
    op->replaceAllUsesWith(extractOp);
    LLVM_DEBUG(llvm::dbgs() << "Post-replacement: " << *parentOp << "\n");

    // Mark all ops in the reduction as visited so we don't try to replace them
    // twice.
    for (Operation *visitedOp : visitedReductionOps) {
      visited.insert(visitedOp);
    }
  }

  void runOnOperation() override {
    DenseSet<Operation *> visited;
    // Traverse the IR in reverse order so that we can eagerly compute backward
    // slices for each operation.
    getOperation()->walk<WalkOrder::PreOrder, ReverseIterator>(
        [&](Operation *op) {
          if (visited.count(op)) {
            return;
          }
          llvm::TypeSwitch<Operation &>(*op)
              .Case<arith::AddIOp>([&](auto arithOp) {
                tryReplace<arith::AddIOp>(arithOp, visited);
              })
              .Case<arith::MulIOp>([&](auto arithOp) {
                tryReplace<arith::MulIOp>(arithOp, visited);
              });
        });
  }
};

}  // namespace tensor_ext
}  // namespace heir
}  // namespace mlir
