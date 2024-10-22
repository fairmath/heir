#ifndef LIB_TRANSFORMS_FORWARDINSERTTOEXTRACT_FORWARDINSERTTOEXTRACT_H_
#define LIB_TRANSFORMS_FORWARDINSERTTOEXTRACT_FORWARDINSERTTOEXTRACT_H_

#include "mlir/include/mlir/Dialect/Affine/Utils.h"      // from @llvm-project
#include "mlir/include/mlir/Dialect/Tensor/IR/Tensor.h"  // from @llvm-project
#include "mlir/include/mlir/IR/Dominance.h"              // from @llvm-project
#include "mlir/include/mlir/IR/MLIRContext.h"            // from @llvm-project
#include "mlir/include/mlir/IR/PatternMatch.h"           // from @llvm-project
#include "mlir/include/mlir/Pass/Pass.h"                 // from @llvm-project
#include "mlir/include/mlir/Support/LLVM.h"              // from @llvm-project
#include "mlir/include/mlir/Transforms/Passes.h"         // from @llvm-project

namespace mlir {
namespace heir {

#define GEN_PASS_DECL
#include "lib/Transforms/ForwardInsertToExtract/ForwardInsertToExtract.h.inc"

#define GEN_PASS_REGISTRATION
#include "lib/Transforms/ForwardInsertToExtract/ForwardInsertToExtract.h.inc"

struct ForwardSingleInsertToExtract
    : public OpRewritePattern<tensor::ExtractOp> {
  ForwardSingleInsertToExtract(mlir::MLIRContext *context, DominanceInfo &dom)
      : OpRewritePattern<tensor::ExtractOp>(context, 3), dominanceInfo(dom) {}

 public:
  LogicalResult matchAndRewrite(tensor::ExtractOp op,
                                PatternRewriter &rewriter) const override;

 private:
  bool isForwardableOp(Operation *potentialInsert,
                       tensor::ExtractOp &extractOp) const;

  DominanceInfo &dominanceInfo;
};

}  // namespace heir
}  // namespace mlir

#endif  // LIB_TRANSFORMS_FORWARDINSERTTOEXTRACT_FORWARDINSERTTOEXTRACT_H_
