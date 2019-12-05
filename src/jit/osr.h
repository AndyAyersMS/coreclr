// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

#ifndef _OSR_H_
#define _OSR_H_

// On-Stack Replacement Data Structures

class ICorJitInfo;

// PatchpointInfo is created by the jit during Tier0 compilations for
// methods with patchpoints. It is associated with the method and is
// passed back by the runtime to the jit for OSR requests on the
// method.
//
// This version of the OSRInfo expects that all patchpoints will
// report the same live set, regardless of IL offset. So just one
// record covers an entire method, no matter how many patchpoints
// it might contain.

class PatchpointInfo
{
public:
    // Allocate and initialize runtime storage for a patchpoint info record
    static PatchpointInfo* Allocate(ICorJitInfo* jitInterface, unsigned localCount, unsigned ilSize, int fpToSpDelta);

    // Total size of this patchpoint info record, in bytes
    unsigned PatchpointInfoSize() const
    {
        return m_patchpointInfoSize;
    }

    // IL Size of the original method
    unsigned ILSize() const
    {
        return m_ilSize;
    }

    // FP to SP delta of the original method
    int FpToSpDelta() const
    {
        return m_fpToSpDelta;
    }

    // Number of locals in the original method (including special locals)
    unsigned NumberOfLocals() const
    {
        return m_numberOfLocals;
    }

    // Original method caller SP offset for generic context arg
    int GenericContextArgOffset() const
    {
        return m_genericContextArgOffset;
    }

    void SetGenericContextArgOffset(int offset)
    {
        m_genericContextArgOffset = offset;
    }

    // Original method caller SP offset for security cookie
    int SecurityCookieOffset() const
    {
        return m_securityCookieOffset;
    }

    bool HasSecurityCookie() const
    {
        return m_securityCookieOffset != -1;
    }

    void SetSecurityCookieOffset(int offset)
    {
        m_securityCookieOffset = offset;
    }

    // True if this local was address exposed in the original method
    bool IsExposed(unsigned localNum) const;
    void SetIsExposed(unsigned localNum);

    // FP relative offset of this local in the original method
    int Offset(unsigned localNum) const;
    void SetOffset(unsigned localNum, int offset);

private:
    enum
    {
        EXPOSURE_MASK = 0x1
    };

    unsigned m_patchpointInfoSize;
    unsigned m_ilSize;
    unsigned m_numberOfLocals;
    int      m_fpToSpDelta;
    int      m_genericContextArgOffset;
    int      m_securityCookieOffset;
    int      m_offsetAndExposureData[];
};

#endif // _OSR_H_
