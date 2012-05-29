//===--- ScratchBuffer.h - Scratch space for forming tokens -----*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines the ScratchBuffer interface..
//
//===----------------------------------------------------------------------===//

#ifndef MLANG_LEX_SCRATCH_BUFFER_H_
#define MLANG_LEX_SCRATCH_BUFFER_H_

#include "mlang/Basic/SourceLocation.h"

namespace mlang {
class SourceManager;

/// ScratchBuffer - This class exposes a simple interface for the dynamic
/// construction of tokens.  This is used for builtin macros (e.g. __LINE__) as
/// well as token pasting, etc.
class ScratchBuffer {
	SourceManager &SourceMgr;
	char *CurBuffer;
	SourceLocation BufferStartLoc;
	unsigned BytesUsed;
public:
	ScratchBuffer(SourceManager &SM);

	/// getToken - Splat the specified text into a temporary MemoryBuffer and
	/// return a SourceLocation that refers to the token.  This is just like the
	/// previous method, but returns a location that indicates the physloc of the
	/// token.
	SourceLocation
			getToken(const char *Buf, unsigned Len, const char *&DestPtr);

private:
	void AllocScratchBuffer(unsigned RequestLen);
};
} // end namespace mlang

#endif /* MLANG_LEX_SCRATCH_BUFFER_H_ */
