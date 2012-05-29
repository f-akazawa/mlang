//===--- ClassDefnAttrs.h - XXXXXXX for Mlang  ---------------------*- C++ -*-===//
//
// Copyright (C) 2010 yabin @ CGCL
// HuaZhong University of Science and Technology, China
// 
//===----------------------------------------------------------------------===//
//
//  This file defines XXXXXXX.
//
//===----------------------------------------------------------------------===//

#ifndef CLASSDEFNATTRS_H_
#define CLASSDEFNATTRS_H_

namespace mlang {

enum ClassAttrs {
	CA_Hidden, CA_InferiorClasses, CA_ConstructOnLoad, CA_Sealed
};

enum ClassPropertyAttrs {
	CPA_AbortSet, CPA_Abstract, CPA_Access, CPA_Constant,
	CPA_Dependent, CPA_GetAccess, CPA_GetObservable,
	CPA_Hidden, CPA_SetAccess, CPA_SetObservable, CPA_Transient
};

enum ClassMethodAttrs {
	CMA_Abstract, CMA_Access, CMA_Hidden, CMA_Sealed, CMA_Static
};

enum ClassEventAttrs {
	CEA_Hidden, CEA_ListenAccess, CEA_NotifyAccess
};
} // end namespace mlang

#endif /* CLASSDEFNATTRS_H_ */
