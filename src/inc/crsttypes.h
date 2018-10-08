//
// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.
//

#ifndef __CRST_TYPES_INCLUDED
#define __CRST_TYPES_INCLUDED

// **** THIS IS AN AUTOMATICALLY GENERATED HEADER FILE -- DO NOT EDIT!!! ****

// This file describes the range of Crst types available and their mapping to a numeric level (used by the
// runtime in debug mode to validate we're deadlock free). To modify these settings edit the
// file:CrstTypes.def file and run the clr\bin\CrstTypeTool utility to generate a new version of this file.

// Each Crst type is declared as a value in the following CrstType enum.
enum CrstType
{
    CrstActiveLoaderAllocators = 0,
    CrstAllowedFiles = 1,
    CrstAppDomainCache = 2,
    CrstAppDomainHandleTable = 3,
    CrstArgBasedStubCache = 4,
    CrstAssemblyDependencyGraph = 5,
    CrstAssemblyIdentityCache = 6,
    CrstAssemblyList = 7,
    CrstAssemblyLoader = 8,
    CrstAvailableClass = 9,
    CrstAvailableParamTypes = 10,
    CrstBaseDomain = 11,
    CrstCCompRC = 12,
    CrstCer = 13,
    CrstClassFactInfoHash = 14,
    CrstClassInit = 15,
    CrstClrNotification = 16,
    CrstCLRPrivBinderMaps = 17,
    CrstCLRPrivBinderMapsAdd = 18,
    CrstCodeFragmentHeap = 19,
    CrstCOMWrapperCache = 20,
    CrstConnectionNameTable = 21,
    CrstContexts = 22,
    CrstCoreCLRBinderLog = 23,
    CrstCrstCLRPrivBinderLocalWinMDPath = 24,
    CrstCSPCache = 25,
    CrstDataTest1 = 26,
    CrstDataTest2 = 27,
    CrstDbgTransport = 28,
    CrstDeadlockDetection = 29,
    CrstDebuggerController = 30,
    CrstDebuggerFavorLock = 31,
    CrstDebuggerHeapExecMemLock = 32,
    CrstDebuggerHeapLock = 33,
    CrstDebuggerJitInfo = 34,
    CrstDebuggerMutex = 35,
    CrstDelegateToFPtrHash = 36,
    CrstDomainLocalBlock = 37,
    CrstDynamicIL = 38,
    CrstDynamicMT = 39,
    CrstDynLinkZapItems = 40,
    CrstEtwTypeLogHash = 41,
    CrstEventPipe = 42,
    CrstEventStore = 43,
    CrstException = 44,
    CrstExecuteManLock = 45,
    CrstExecuteManRangeLock = 46,
    CrstFCall = 47,
    CrstFriendAccessCache = 48,
    CrstFuncPtrStubs = 49,
    CrstFusionAppCtx = 50,
    CrstFusionAssemblyDownload = 51,
    CrstFusionBindContext = 52,
    CrstFusionBindResult = 53,
    CrstFusionClb = 54,
    CrstFusionClosure = 55,
    CrstFusionClosureGraph = 56,
    CrstFusionConfigSettings = 57,
    CrstFusionDownload = 58,
    CrstFusionIsoLibInit = 59,
    CrstFusionLoadContext = 60,
    CrstFusionLog = 61,
    CrstFusionNgenIndex = 62,
    CrstFusionNgenIndexPool = 63,
    CrstFusionPcyCache = 64,
    CrstFusionPolicyConfigPool = 65,
    CrstFusionSingleUse = 66,
    CrstFusionWarningLog = 67,
    CrstGCCover = 68,
    CrstGCMemoryPressure = 69,
    CrstGlobalStrLiteralMap = 70,
    CrstHandleTable = 71,
    CrstHostAssemblyMap = 72,
    CrstHostAssemblyMapAdd = 73,
    CrstIbcProfile = 74,
    CrstIJWFixupData = 75,
    CrstIJWHash = 76,
    CrstILFingerprintCache = 77,
    CrstILStubGen = 78,
    CrstInlineTrackingMap = 79,
    CrstInstMethodHashTable = 80,
    CrstInterfaceVTableMap = 81,
    CrstInterop = 82,
    CrstInteropData = 83,
    CrstIOThreadpoolWorker = 84,
    CrstIsJMCMethod = 85,
    CrstISymUnmanagedReader = 86,
    CrstJit = 87,
    CrstJitGenericHandleCache = 88,
    CrstJitPerf = 89,
    CrstJumpStubCache = 90,
    CrstLeafLock = 91,
    CrstListLock = 92,
    CrstLoaderAllocator = 93,
    CrstLoaderAllocatorReferences = 94,
    CrstLoaderHeap = 95,
    CrstMda = 96,
    CrstMetadataTracker = 97,
    CrstModIntPairList = 98,
    CrstModule = 99,
    CrstModuleFixup = 100,
    CrstModuleLookupTable = 101,
    CrstMulticoreJitHash = 102,
    CrstMulticoreJitManager = 103,
    CrstMUThunkHash = 104,
    CrstNativeBinderInit = 105,
    CrstNativeImageCache = 106,
    CrstNls = 107,
    CrstNotifyGdb = 108,
    CrstObjectList = 109,
    CrstOnEventManager = 110,
    CrstPatchEntryPoint = 111,
    CrstPEFileSecurityManager = 112,
    CrstPEImage = 113,
    CrstPEImagePDBStream = 114,
    CrstPendingTypeLoadEntry = 115,
    CrstPinHandle = 116,
    CrstPinnedByrefValidation = 117,
    CrstProfilerGCRefDataFreeList = 118,
    CrstProfilingAPIStatus = 119,
    CrstPublisherCertificate = 120,
    CrstRCWCache = 121,
    CrstRCWCleanupList = 122,
    CrstRCWRefCache = 123,
    CrstReadyToRunEntryPointToMethodDescMap = 124,
    CrstReDacl = 125,
    CrstReflection = 126,
    CrstReJITDomainTable = 127,
    CrstReJITGlobalRequest = 128,
    CrstReJITSharedDomainTable = 129,
    CrstRemoting = 130,
    CrstRetThunkCache = 131,
    CrstRWLock = 132,
    CrstSavedExceptionInfo = 133,
    CrstSaveModuleProfileData = 134,
    CrstSecurityPolicyCache = 135,
    CrstSecurityPolicyInit = 136,
    CrstSecurityStackwalkCache = 137,
    CrstSharedAssemblyCreate = 138,
    CrstSharedBaseDomain = 139,
    CrstSigConvert = 140,
    CrstSingleUseLock = 141,
    CrstSpecialStatics = 142,
    CrstSqmManager = 143,
    CrstStackSampler = 144,
    CrstStressLog = 145,
    CrstStrongName = 146,
    CrstStubCache = 147,
    CrstStubDispatchCache = 148,
    CrstStubUnwindInfoHeapSegments = 149,
    CrstSyncBlockCache = 150,
    CrstSyncHashLock = 151,
    CrstSystemBaseDomain = 152,
    CrstSystemDomain = 153,
    CrstSystemDomainDelayedUnloadList = 154,
    CrstThreadIdDispenser = 155,
    CrstThreadpoolEventCache = 156,
    CrstThreadpoolTimerQueue = 157,
    CrstThreadpoolWaitThreads = 158,
    CrstThreadpoolWorker = 159,
    CrstThreadStaticDataHashTable = 160,
    CrstThreadStore = 161,
    CrstTieredCompilation = 162,
    CrstTPMethodTable = 163,
    CrstTypeEquivalenceMap = 164,
    CrstTypeIDMap = 165,
    CrstUMEntryThunkCache = 166,
    CrstUMThunkHash = 167,
    CrstUniqueStack = 168,
    CrstUnresolvedClassLock = 169,
    CrstUnwindInfoTableLock = 170,
    CrstVSDIndirectionCellLock = 171,
    CrstWinRTFactoryCache = 172,
    CrstWrapperTemplate = 173,
    kNumberOfCrstTypes = 174
};

#endif // __CRST_TYPES_INCLUDED

// Define some debug data in one module only -- vm\crst.cpp.
#if defined(__IN_CRST_CPP) && defined(_DEBUG)

// An array mapping CrstType to level.
int g_rgCrstLevelMap[] =
{
    15,         // CrstActiveLoaderAllocators
    9,          // CrstAllowedFiles
    9,          // CrstAppDomainCache
    13,         // CrstAppDomainHandleTable
    0,          // CrstArgBasedStubCache
    0,          // CrstAssemblyDependencyGraph
    0,          // CrstAssemblyIdentityCache
    0,          // CrstAssemblyList
    7,          // CrstAssemblyLoader
    3,          // CrstAvailableClass
    6,          // CrstAvailableParamTypes
    7,          // CrstBaseDomain
    -1,         // CrstCCompRC
    9,          // CrstCer
    11,         // CrstClassFactInfoHash
    8,          // CrstClassInit
    -1,         // CrstClrNotification
    0,          // CrstCLRPrivBinderMaps
    3,          // CrstCLRPrivBinderMapsAdd
    6,          // CrstCodeFragmentHeap
    4,          // CrstCOMWrapperCache
    0,          // CrstConnectionNameTable
    17,         // CrstContexts
    -1,         // CrstCoreCLRBinderLog
    0,          // CrstCrstCLRPrivBinderLocalWinMDPath
    7,          // CrstCSPCache
    3,          // CrstDataTest1
    0,          // CrstDataTest2
    0,          // CrstDbgTransport
    0,          // CrstDeadlockDetection
    -1,         // CrstDebuggerController
    3,          // CrstDebuggerFavorLock
    0,          // CrstDebuggerHeapExecMemLock
    0,          // CrstDebuggerHeapLock
    4,          // CrstDebuggerJitInfo
    11,         // CrstDebuggerMutex
    0,          // CrstDelegateToFPtrHash
    15,         // CrstDomainLocalBlock
    0,          // CrstDynamicIL
    3,          // CrstDynamicMT
    3,          // CrstDynLinkZapItems
    7,          // CrstEtwTypeLogHash
    17,         // CrstEventPipe
    0,          // CrstEventStore
    0,          // CrstException
    7,          // CrstExecuteManLock
    0,          // CrstExecuteManRangeLock
    3,          // CrstFCall
    7,          // CrstFriendAccessCache
    7,          // CrstFuncPtrStubs
    9,          // CrstFusionAppCtx
    7,          // CrstFusionAssemblyDownload
    5,          // CrstFusionBindContext
    0,          // CrstFusionBindResult
    0,          // CrstFusionClb
    16,         // CrstFusionClosure
    10,         // CrstFusionClosureGraph
    0,          // CrstFusionConfigSettings
    0,          // CrstFusionDownload
    0,          // CrstFusionIsoLibInit
    5,          // CrstFusionLoadContext
    4,          // CrstFusionLog
    7,          // CrstFusionNgenIndex
    7,          // CrstFusionNgenIndexPool
    0,          // CrstFusionPcyCache
    4,          // CrstFusionPolicyConfigPool
    5,          // CrstFusionSingleUse
    6,          // CrstFusionWarningLog
    10,         // CrstGCCover
    0,          // CrstGCMemoryPressure
    11,         // CrstGlobalStrLiteralMap
    1,          // CrstHandleTable
    0,          // CrstHostAssemblyMap
    3,          // CrstHostAssemblyMapAdd
    0,          // CrstIbcProfile
    9,          // CrstIJWFixupData
    0,          // CrstIJWHash
    5,          // CrstILFingerprintCache
    7,          // CrstILStubGen
    3,          // CrstInlineTrackingMap
    16,         // CrstInstMethodHashTable
    0,          // CrstInterfaceVTableMap
    17,         // CrstInterop
    4,          // CrstInteropData
    11,         // CrstIOThreadpoolWorker
    0,          // CrstIsJMCMethod
    7,          // CrstISymUnmanagedReader
    8,          // CrstJit
    0,          // CrstJitGenericHandleCache
    -1,         // CrstJitPerf
    6,          // CrstJumpStubCache
    0,          // CrstLeafLock
    -1,         // CrstListLock
    14,         // CrstLoaderAllocator
    15,         // CrstLoaderAllocatorReferences
    0,          // CrstLoaderHeap
    0,          // CrstMda
    -1,         // CrstMetadataTracker
    0,          // CrstModIntPairList
    4,          // CrstModule
    14,         // CrstModuleFixup
    3,          // CrstModuleLookupTable
    0,          // CrstMulticoreJitHash
    11,         // CrstMulticoreJitManager
    0,          // CrstMUThunkHash
    -1,         // CrstNativeBinderInit
    -1,         // CrstNativeImageCache
    0,          // CrstNls
    0,          // CrstNotifyGdb
    2,          // CrstObjectList
    0,          // CrstOnEventManager
    0,          // CrstPatchEntryPoint
    0,          // CrstPEFileSecurityManager
    4,          // CrstPEImage
    0,          // CrstPEImagePDBStream
    18,         // CrstPendingTypeLoadEntry
    0,          // CrstPinHandle
    0,          // CrstPinnedByrefValidation
    0,          // CrstProfilerGCRefDataFreeList
    0,          // CrstProfilingAPIStatus
    0,          // CrstPublisherCertificate
    3,          // CrstRCWCache
    0,          // CrstRCWCleanupList
    3,          // CrstRCWRefCache
    4,          // CrstReadyToRunEntryPointToMethodDescMap
    0,          // CrstReDacl
    9,          // CrstReflection
    7,          // CrstReJITDomainTable
    13,         // CrstReJITGlobalRequest
    9,          // CrstReJITSharedDomainTable
    19,         // CrstRemoting
    3,          // CrstRetThunkCache
    0,          // CrstRWLock
    3,          // CrstSavedExceptionInfo
    0,          // CrstSaveModuleProfileData
    0,          // CrstSecurityPolicyCache
    3,          // CrstSecurityPolicyInit
    0,          // CrstSecurityStackwalkCache
    4,          // CrstSharedAssemblyCreate
    7,          // CrstSharedBaseDomain
    3,          // CrstSigConvert
    5,          // CrstSingleUseLock
    0,          // CrstSpecialStatics
    0,          // CrstSqmManager
    0,          // CrstStackSampler
    -1,         // CrstStressLog
    0,          // CrstStrongName
    5,          // CrstStubCache
    0,          // CrstStubDispatchCache
    4,          // CrstStubUnwindInfoHeapSegments
    3,          // CrstSyncBlockCache
    0,          // CrstSyncHashLock
    0,          // CrstSystemBaseDomain
    12,         // CrstSystemDomain
    0,          // CrstSystemDomainDelayedUnloadList
    0,          // CrstThreadIdDispenser
    0,          // CrstThreadpoolEventCache
    7,          // CrstThreadpoolTimerQueue
    7,          // CrstThreadpoolWaitThreads
    11,         // CrstThreadpoolWorker
    4,          // CrstThreadStaticDataHashTable
    10,         // CrstThreadStore
    9,          // CrstTieredCompilation
    9,          // CrstTPMethodTable
    3,          // CrstTypeEquivalenceMap
    7,          // CrstTypeIDMap
    3,          // CrstUMEntryThunkCache
    0,          // CrstUMThunkHash
    3,          // CrstUniqueStack
    7,          // CrstUnresolvedClassLock
    3,          // CrstUnwindInfoTableLock
    3,          // CrstVSDIndirectionCellLock
    3,          // CrstWinRTFactoryCache
    3,          // CrstWrapperTemplate
};

// An array mapping CrstType to a stringized name.
LPCSTR g_rgCrstNameMap[] =
{
    "CrstActiveLoaderAllocators",
    "CrstAllowedFiles",
    "CrstAppDomainCache",
    "CrstAppDomainHandleTable",
    "CrstArgBasedStubCache",
    "CrstAssemblyDependencyGraph",
    "CrstAssemblyIdentityCache",
    "CrstAssemblyList",
    "CrstAssemblyLoader",
    "CrstAvailableClass",
    "CrstAvailableParamTypes",
    "CrstBaseDomain",
    "CrstCCompRC",
    "CrstCer",
    "CrstClassFactInfoHash",
    "CrstClassInit",
    "CrstClrNotification",
    "CrstCLRPrivBinderMaps",
    "CrstCLRPrivBinderMapsAdd",
    "CrstCodeFragmentHeap",
    "CrstCOMWrapperCache",
    "CrstConnectionNameTable",
    "CrstContexts",
    "CrstCoreCLRBinderLog",
    "CrstCrstCLRPrivBinderLocalWinMDPath",
    "CrstCSPCache",
    "CrstDataTest1",
    "CrstDataTest2",
    "CrstDbgTransport",
    "CrstDeadlockDetection",
    "CrstDebuggerController",
    "CrstDebuggerFavorLock",
    "CrstDebuggerHeapExecMemLock",
    "CrstDebuggerHeapLock",
    "CrstDebuggerJitInfo",
    "CrstDebuggerMutex",
    "CrstDelegateToFPtrHash",
    "CrstDomainLocalBlock",
    "CrstDynamicIL",
    "CrstDynamicMT",
    "CrstDynLinkZapItems",
    "CrstEtwTypeLogHash",
    "CrstEventPipe",
    "CrstEventStore",
    "CrstException",
    "CrstExecuteManLock",
    "CrstExecuteManRangeLock",
    "CrstFCall",
    "CrstFriendAccessCache",
    "CrstFuncPtrStubs",
    "CrstFusionAppCtx",
    "CrstFusionAssemblyDownload",
    "CrstFusionBindContext",
    "CrstFusionBindResult",
    "CrstFusionClb",
    "CrstFusionClosure",
    "CrstFusionClosureGraph",
    "CrstFusionConfigSettings",
    "CrstFusionDownload",
    "CrstFusionIsoLibInit",
    "CrstFusionLoadContext",
    "CrstFusionLog",
    "CrstFusionNgenIndex",
    "CrstFusionNgenIndexPool",
    "CrstFusionPcyCache",
    "CrstFusionPolicyConfigPool",
    "CrstFusionSingleUse",
    "CrstFusionWarningLog",
    "CrstGCCover",
    "CrstGCMemoryPressure",
    "CrstGlobalStrLiteralMap",
    "CrstHandleTable",
    "CrstHostAssemblyMap",
    "CrstHostAssemblyMapAdd",
    "CrstIbcProfile",
    "CrstIJWFixupData",
    "CrstIJWHash",
    "CrstILFingerprintCache",
    "CrstILStubGen",
    "CrstInlineTrackingMap",
    "CrstInstMethodHashTable",
    "CrstInterfaceVTableMap",
    "CrstInterop",
    "CrstInteropData",
    "CrstIOThreadpoolWorker",
    "CrstIsJMCMethod",
    "CrstISymUnmanagedReader",
    "CrstJit",
    "CrstJitGenericHandleCache",
    "CrstJitPerf",
    "CrstJumpStubCache",
    "CrstLeafLock",
    "CrstListLock",
    "CrstLoaderAllocator",
    "CrstLoaderAllocatorReferences",
    "CrstLoaderHeap",
    "CrstMda",
    "CrstMetadataTracker",
    "CrstModIntPairList",
    "CrstModule",
    "CrstModuleFixup",
    "CrstModuleLookupTable",
    "CrstMulticoreJitHash",
    "CrstMulticoreJitManager",
    "CrstMUThunkHash",
    "CrstNativeBinderInit",
    "CrstNativeImageCache",
    "CrstNls",
    "CrstNotifyGdb",
    "CrstObjectList",
    "CrstOnEventManager",
    "CrstPatchEntryPoint",
    "CrstPEFileSecurityManager",
    "CrstPEImage",
    "CrstPEImagePDBStream",
    "CrstPendingTypeLoadEntry",
    "CrstPinHandle",
    "CrstPinnedByrefValidation",
    "CrstProfilerGCRefDataFreeList",
    "CrstProfilingAPIStatus",
    "CrstPublisherCertificate",
    "CrstRCWCache",
    "CrstRCWCleanupList",
    "CrstRCWRefCache",
    "CrstReadyToRunEntryPointToMethodDescMap",
    "CrstReDacl",
    "CrstReflection",
    "CrstReJITDomainTable",
    "CrstReJITGlobalRequest",
    "CrstReJITSharedDomainTable",
    "CrstRemoting",
    "CrstRetThunkCache",
    "CrstRWLock",
    "CrstSavedExceptionInfo",
    "CrstSaveModuleProfileData",
    "CrstSecurityPolicyCache",
    "CrstSecurityPolicyInit",
    "CrstSecurityStackwalkCache",
    "CrstSharedAssemblyCreate",
    "CrstSharedBaseDomain",
    "CrstSigConvert",
    "CrstSingleUseLock",
    "CrstSpecialStatics",
    "CrstSqmManager",
    "CrstStackSampler",
    "CrstStressLog",
    "CrstStrongName",
    "CrstStubCache",
    "CrstStubDispatchCache",
    "CrstStubUnwindInfoHeapSegments",
    "CrstSyncBlockCache",
    "CrstSyncHashLock",
    "CrstSystemBaseDomain",
    "CrstSystemDomain",
    "CrstSystemDomainDelayedUnloadList",
    "CrstThreadIdDispenser",
    "CrstThreadpoolEventCache",
    "CrstThreadpoolTimerQueue",
    "CrstThreadpoolWaitThreads",
    "CrstThreadpoolWorker",
    "CrstThreadStaticDataHashTable",
    "CrstThreadStore",
    "CrstTieredCompilation",
    "CrstTPMethodTable",
    "CrstTypeEquivalenceMap",
    "CrstTypeIDMap",
    "CrstUMEntryThunkCache",
    "CrstUMThunkHash",
    "CrstUniqueStack",
    "CrstUnresolvedClassLock",
    "CrstUnwindInfoTableLock",
    "CrstVSDIndirectionCellLock",
    "CrstWinRTFactoryCache",
    "CrstWrapperTemplate",
};

// Define a special level constant for unordered locks.
#define CRSTUNORDERED (-1)

// Define inline helpers to map Crst types to names and levels.
inline static int GetCrstLevel(CrstType crstType)
{
    LIMITED_METHOD_CONTRACT;
    _ASSERTE(crstType >= 0 && crstType < kNumberOfCrstTypes);
    return g_rgCrstLevelMap[crstType];
}
inline static LPCSTR GetCrstName(CrstType crstType)
{
    LIMITED_METHOD_CONTRACT;
    _ASSERTE(crstType >= 0 && crstType < kNumberOfCrstTypes);
    return g_rgCrstNameMap[crstType];
}

#endif // defined(__IN_CRST_CPP) && defined(_DEBUG)
