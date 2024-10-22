#include "lib/Dialect/TensorExt/IR/TensorExtOps.h"

#include "llvm/include/llvm/ADT/STLExtras.h"             // from @llvm-project
#include "mlir/include/mlir/Dialect/Arith/IR/Arith.h"    // from @llvm-project
#include "mlir/include/mlir/Dialect/Tensor/IR/Tensor.h"  // from @llvm-project
#include "mlir/include/mlir/IR/MLIRContext.h"            // from @llvm-project
#include "mlir/include/mlir/IR/Matchers.h"               // from @llvm-project
#include "mlir/include/mlir/IR/PatternMatch.h"           // from @llvm-project
#include "mlir/include/mlir/Support/LogicalResult.h"     // from @llvm-project

namespace mlir {
namespace heir {
namespace tensor_ext {

// Kept inside a namespace because it generates a function called
// populateWithGenerated, which can conflict with other generated patterns.
#include "lib/Dialect/TensorExt/IR/TensorExtCanonicalization.cpp.inc"

void RotateOp::getCanonicalizationPatterns(RewritePatternSet &results,
                                           MLIRContext *context) {
  populateWithGenerated(results);
}

LogicalResult RotateOp::verify() {
  auto x = getTensor().getType();
  // TODO(#924): Currently RotateOp only supports rotating a 1-D vector, or a
  // vector with only one non-unit dimension that is treated as the major
  // dimension.
  if (x.getRank() != 1) {
    if (llvm::count_if(x.getShape(), [](auto dim) { return dim != 1; }) != 1) {
      return emitOpError() << "requires a 1-D input tensor or tensor with "
                              "single non-unit dimension, but found "
                           << x;
    }
  }
  return success();
}

}  // namespace tensor_ext
}  // namespace heir
}  // namespace mlir
