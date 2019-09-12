
// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#ifndef _OSR_H_
#define _OSR_H_

// On-Stack Replacement Data Structures

class ICorJitInfo;

// OSRInfo is created by Tier0 compilations for methods with
// patchpoints. It is associated with the method and is passed
// back by the runtime to the jit for OSR requests on the Tier0
// method.

class OSRInfo
{
public:
    OSRInfo* Allocate(ICorJitInfo* jitInterface, int localCount, int ilSize, int fpToSpDelta);

    int ILSize() const { return m_ilSize; }
    int FpToSpDelta() const { return m_fpToSpDelta; }
    int NumberOfLocals() const { return m_numberOfLocals; }
    bool IsExpsed(int localNum) const;
    int Offset(int localNum) const;

    void SetIsExposed(int localNum);
    void SetOffset(int localNum, int offset);

private:

    const int exposureMask = 0x1;
    int m_ilSize;
    int m_fpToSpDelta;
    int m_numberOfLocals;
    int[] m_offsetAndExposureData;
};

#endif // _OSR_H_

