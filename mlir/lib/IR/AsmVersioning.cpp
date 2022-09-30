//===- AsmVersioning.cpp - MLIR DialectVersionConverter Implementation --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file implements the MLIR DialectVersionConverter, which is used to implement
// upgrade and downgrade hooks.
//
//===----------------------------------------------------------------------===//

#include "mlir/IR/Attributes.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Debug.h"

#define DEBUG_TYPE "mlir-asm-version"

using namespace mlir;

FailureOr<Attribute> DialectVersionConverter::applyConversion(
    Operation *op, Attribute const &version,
    llvm::StringMap<llvm::SmallVector<OpConversionAttributePair>> &map,
    std::function<bool(Attribute, Attribute)> const &comparisonFn) {

  // Find if any conversions for this given op are registered.
  OperationName mnemonic = op->getName();
  auto it = map.find(mnemonic.stripDialect());

  // No conversions, return original version.
  if (it == map.end()) {
    return version;
  }

  // Sort the conversions for this given attribute and see if any can be
  // applied.
  // This will either sort in ascending or descending order depending on
  // comparisonFn.
  //
  // Sort on every conversion may be costly, can consider refactoring.
  llvm::SmallVector<OpConversionAttributePair> &conversions = it->second;
  std::sort(conversions.begin(), conversions.end(),
            [&](OpConversionAttributePair const &a,
                OpConversionAttributePair const &b) {
              return comparisonFn(a.version, b.version);
            });

  // Iterate over conversions, if one is greater than version argument, apply
  // it and modify version.
  for (auto &convPair : conversions) {
    LLVM_DEBUG(llvm::dbgs() << "Trying to apply v" << convPair.version << " to "
                            << op->getName().getStringRef() << '\n');
    if (comparisonFn(version, convPair.version)) {
      // If conversion was attempted, return failure or new attribute version.
      if (failed(convPair.conversion(op, version))) {
        LLVM_DEBUG(llvm::dbgs() << "Op failed to apply conversion "
                                << convPair.version << '\n');
        return failure();
      }
      assert(convPair.version != version); // Must always increase/decrease.
      return convPair.version;             // Return the new version
    }
  }

  // No conversions applied, return original version.
  return version;
}

namespace {
LogicalResult walkAndApply(
    Operation *topLevelOp,
    std::function<Attribute(Operation *)> const &getOpVersionFn,
    std::function<FailureOr<Attribute>(Operation *, Attribute const &,
                                       DialectVersionConverter *)> const &cb) {
  // Perform any upgrades
  auto walkRes = topLevelOp->walk([&](Operation *op) {
    LLVM_DEBUG(llvm::dbgs() << "Getting dialect version for "
                            << op->getName().getStringRef() << '\n');
    Attribute version = getOpVersionFn(op);
    if (!version)
      return WalkResult::advance();

    FailureOr<Attribute> attrOrFail(version);

    do {
      // Upgrade failed, interrupt and error.
      // Get this every time in case upgrade changes op dialect.
      LLVM_DEBUG(llvm::dbgs() << "Checking conversions for "
                              << op->getName().getStringRef() << '\n');
      auto *dialect = op->getDialect();
      if (!dialect)
        return WalkResult::advance();

      auto *asmIface = dialect->getRegisteredInterface<OpAsmDialectInterface>();
      if (!asmIface || !asmIface->getDialectVersionConverter())
        return WalkResult::advance();

      LLVM_DEBUG(llvm::dbgs() << "Attempting to apply conversion to "
                              << op->getName().getStringRef() << '\n');
      // Iteratively apply upgrades until none are applied.
      version = *attrOrFail; // Update the minimum version.
      attrOrFail = cb(op, version, asmIface->getDialectVersionConverter());

      if (failed(attrOrFail)) {
        // Upgrade failed, interrupt and error.
        LLVM_DEBUG(llvm::dbgs() << "Op failed to apply conversion.\n");
        return WalkResult::interrupt();
      }

      LLVM_DEBUG(llvm::dbgs() << "Applied conversion = "
                              << (*attrOrFail != version) << '\n');
    } while (*attrOrFail != version);
    return WalkResult::advance();
  });

  return success(/*isSuccess=*/!walkRes.wasInterrupted());
}

Attribute getOpVersionFromDialectResources(Operation * op) {
  char *envVar = std::getenv("OP_VERSION");
  auto versionStr = std::string(envVar ? envVar : "");
  if (versionStr.empty())
    versionStr = "1";
  return Builder(op->getContext()).getI32IntegerAttr(atoi(versionStr.c_str()));
}

Attribute getOpVersionFromDialectProducerVersion(Operation * op) {
  auto *dialect = op->getDialect();
  if (!dialect)
    return Attribute();

  auto *asmIface = dialect->getRegisteredInterface<OpAsmDialectInterface>();
  if (!asmIface || !asmIface->getDialectVersionConverter())
    return Attribute();

  return asmIface->getDialectVersionConverter()->getProducerVersion();
}
} // namespace

LogicalResult DialectVersionConverter::applyOpUpgrades(Operation *topLevelOp) {
  // FIXME: This version number would need to come from the dialect_resources:
  return walkAndApply(topLevelOp, getOpVersionFromDialectResources,
                      [](Operation *op, Attribute const &attr,
                         DialectVersionConverter *converter) {
                        return converter->upgrade(op, attr);
                      });
}

LogicalResult DialectVersionConverter::applyOpDowngrades(Operation *topLevelOp) {
  // Version function should return the producer version of the dialect.
  return walkAndApply(topLevelOp, getOpVersionFromDialectProducerVersion,
                      [](Operation *op, Attribute const &attr,
                         DialectVersionConverter *converter) {
                        return converter->downgrade(op, attr);
                      });
}

