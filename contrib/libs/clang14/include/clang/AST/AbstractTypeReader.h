#pragma once

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

//==--- AbstractTypeReader.h - Abstract deserialization for types ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_ABSTRACTTYPEREADER_H
#define LLVM_CLANG_AST_ABSTRACTTYPEREADER_H

#include "clang/AST/Type.h"
#include "clang/AST/AbstractBasicReader.h"

namespace clang {
namespace serialization {

// template <class PropertyReader>
// class AbstractTypeReader {
// public:
//   AbstractTypeReader(PropertyReader &W);
//   QualType read(Type::TypeClass kind);
// };
//
// The actual class is auto-generated; see ClangASTPropertiesEmitter.cpp.
#include "clang/AST/AbstractTypeReader.inc"

} // end namespace serialization
} // end namespace clang

#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
