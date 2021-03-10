//===--- LLVM.h - Import various common LLVM datatypes ----------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This file forward declares and imports various common LLVM datatypes that
// swift wants to use unqualified.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_BASIC_LLVM_H
#define SWIFT_BASIC_LLVM_H

// Do not proliferate #includes here, require clients to #include their
// dependencies.
// Casting.h has complex templates that cannot be easily forward declared.
//#include "llvm/Support/Casting.h"
#include "Casting.h"
// None.h includes an enumerator that is desired & cannot be forward declared
// without a definition of NoneType.
//#include "llvm/ADT/None.h"
#include "None.h"

// Forward declarations.
namespace llvm {
// Containers.
class StringRef;
class Twine;
template<typename T> class SmallPtrSetImpl;
template<typename T, unsigned N> class SmallPtrSet;
template<typename T> class SmallVectorImpl;
template<typename T, unsigned N> class SmallVector;
template<unsigned N> class SmallString;
template<typename T> class ArrayRef;
template<typename T> class MutableArrayRef;
template<typename T> class TinyPtrVector;
template<typename T> class Optional;
template<typename PT1, typename PT2> class PointerUnion;

// Other common classes.
class raw_ostream;
class APInt;
class APFloat;
} // end namespace llvm

namespace swift {
// Casting operators.
using llvm::cast;
using llvm::cast_or_null;
using llvm::dyn_cast;
using llvm::dyn_cast_or_null;
using llvm::isa;

// Containers.
using llvm::ArrayRef;
using llvm::MutableArrayRef;
using llvm::None;
using llvm::Optional;
using llvm::PointerUnion;
using llvm::SmallPtrSet;
using llvm::SmallPtrSetImpl;
using llvm::SmallString;
using llvm::SmallVector;
using llvm::SmallVectorImpl;
using llvm::StringRef;
using llvm::TinyPtrVector;
using llvm::Twine;

// Other common classes.
using llvm::APFloat;
using llvm::APInt;
using llvm::NoneType;
using llvm::raw_ostream;
} // end namespace swift

#endif // SWIFT_BASIC_LLVM_H
