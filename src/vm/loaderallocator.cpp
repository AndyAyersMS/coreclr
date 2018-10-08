// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.


#include "common.h"
#include "stringliteralmap.h"
#include "virtualcallstub.h"
#include "threadsuspend.h"
#ifndef DACCESS_COMPILE
#include "comdelegate.h"
#endif

//*****************************************************************************
// Used by LoaderAllocator::Init for easier readability.
#ifdef ENABLE_PERF_COUNTERS
#define LOADERHEAP_PROFILE_COUNTER (&(GetPerfCounters().m_Loading.cbLoaderHeapSize))
#else
#define LOADERHEAP_PROFILE_COUNTER (NULL)
#endif

#ifndef CROSSGEN_COMPILE
#define STUBMANAGER_RANGELIST(stubManager) (stubManager::g_pManager->GetRangeList())
#else
#define STUBMANAGER_RANGELIST(stubManager) (NULL)
#endif

UINT64 LoaderAllocator::cLoaderAllocatorsCreated = 1;
SArray<LoaderAllocator*>* LoaderAllocator::s_activeLoaderAllocators = nullptr;
CrstStatic LoaderAllocator::s_ActiveLoaderAllocatorsCrst;

// Init statics
void LoaderAllocator::Init()
{
#ifndef DACCESS_COMPILE
    s_activeLoaderAllocators = new SArray<LoaderAllocator*>();
    s_ActiveLoaderAllocatorsCrst.Init(CrstActiveLoaderAllocators);
#endif // !DACCESS_COMPILE
}

LoaderAllocator::LoaderAllocator()  
{
    LIMITED_METHOD_CONTRACT;

    // initialize all members up front to NULL so that short-circuit failure won't cause invalid values
    m_InitialReservedMemForLoaderHeaps = NULL;
    m_pLowFrequencyHeap = NULL;
    m_pHighFrequencyHeap = NULL;
    m_pStubHeap = NULL;
    m_pPrecodeHeap = NULL;
    m_pExecutableHeap = NULL;
#ifdef FEATURE_READYTORUN
    m_pDynamicHelpersHeap = NULL;
#endif
    m_pFuncPtrStubs = NULL;
    m_hLoaderAllocatorObjectHandle = NULL;
    m_pStringLiteralMap = NULL;
    
    m_cReferences = (UINT32)-1;
    
    m_pFirstDomainAssemblyFromSameALCToDelete = NULL;
    
#ifdef FAT_DISPATCH_TOKENS
    // DispatchTokenFat pointer table for token overflow scenarios. Lazily allocated.
    m_pFatTokenSetLock = NULL;
    m_pFatTokenSet = NULL;
#endif
    
#ifndef CROSSGEN_COMPILE
    m_pVirtualCallStubManager = NULL;
#endif

    m_fGCPressure = false;
    m_fTerminated = false;
    m_fUnloaded = false;
    m_fMarked = false;
    m_pLoaderAllocatorDestroyNext = NULL;
    m_pDomain = NULL;
    m_pCodeHeapInitialAlloc = NULL;
    m_pVSDHeapInitialAlloc = NULL;
    m_pLastUsedCodeHeap = NULL;
    m_pLastUsedDynamicCodeHeap = NULL;
    m_pJumpStubCache = NULL;
    m_IsCollectible = false;

    m_pUMEntryThunkCache = NULL;

    m_nLoaderAllocator = InterlockedIncrement64((LONGLONG *)&LoaderAllocator::cLoaderAllocatorsCreated);
#if !defined(DACCESS_COMPILE) && !defined(CROSSGEN_COMPILE)
    {
        CrstHolder ch(&s_ActiveLoaderAllocatorsCrst);
        s_activeLoaderAllocators->Append(this);
    }
#endif
}

LoaderAllocator::~LoaderAllocator()
{
    CONTRACTL
    {
        DESTRUCTOR_CHECK;
    }
    CONTRACTL_END;
#if !defined(DACCESS_COMPILE) && !defined(CROSSGEN_COMPILE)
    Terminate();

    // Assert that VSD is not still active when the destructor is called.
    _ASSERTE(m_pVirtualCallStubManager == NULL);

     // Code manager is responsible for cleaning up.
    _ASSERTE(m_pJumpStubCache == NULL);

    {
        CrstHolder ch(&s_ActiveLoaderAllocatorsCrst);
        auto iterActiveLoaderAllocators = s_activeLoaderAllocators->Begin();
        auto iterActiveLoaderAllocatorsEnd = s_activeLoaderAllocators->End();
        for (;iterActiveLoaderAllocators != iterActiveLoaderAllocatorsEnd; ++iterActiveLoaderAllocators)
        {
            if ((*iterActiveLoaderAllocators) == this)
            {
                s_activeLoaderAllocators->Delete(iterActiveLoaderAllocators);
                break;
            }
        }
    }
#endif
}

#ifndef DACCESS_COMPILE
//---------------------------------------------------------------------------------------
// 
void LoaderAllocator::AddReference()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;
    
    _ASSERTE((m_cReferences > (UINT32)0) && (m_cReferences != (UINT32)-1));
    FastInterlockIncrement((LONG *)&m_cReferences);
}
#endif //!DACCESS_COMPILE

//---------------------------------------------------------------------------------------
// 
// Adds reference if the native object is alive  - code:LoaderAllocator#AssemblyPhases.
// Returns TRUE if the reference was added.
// 
BOOL LoaderAllocator::AddReferenceIfAlive()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;
    
#ifndef DACCESS_COMPILE
    for (;;)
    {
        // Local snaphost of ref-count
        UINT32 cReferencesLocalSnapshot = m_cReferences;
        _ASSERTE(cReferencesLocalSnapshot != (UINT32)-1);
        
        if (cReferencesLocalSnapshot == 0)
        {   // Ref-count was 0, do not AddRef
            return FALSE;
        }
        
        UINT32 cOriginalReferences = FastInterlockCompareExchange(
            (LONG *)&m_cReferences, 
            cReferencesLocalSnapshot + 1, 
            cReferencesLocalSnapshot);
        
        if (cOriginalReferences == cReferencesLocalSnapshot)
        {   // The exchange happened
            return TRUE;
        }
        // Let's spin till we are the only thread to modify this value
    }
#else //DACCESS_COMPILE
    // DAC won't AddRef
    return IsAlive();
#endif //DACCESS_COMPILE
} // LoaderAllocator::AddReferenceIfAlive

//---------------------------------------------------------------------------------------
// 
BOOL LoaderAllocator::Release()
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
    }
    CONTRACTL_END;
    
    // Only actually destroy the domain assembly when all references to it are gone.
    // This should preserve behavior in the debugger such that an UnloadModule event
    // will occur before the underlying data structure cease functioning.
#ifndef DACCESS_COMPILE
    
    _ASSERTE((m_cReferences > (UINT32)0) && (m_cReferences != (UINT32)-1));
    LONG cNewReferences = FastInterlockDecrement((LONG *)&m_cReferences);
    return (cNewReferences == 0);
#else //DACCESS_COMPILE
    
    return (m_cReferences == (UINT32)0);
#endif //DACCESS_COMPILE
} // LoaderAllocator::Release

#ifndef DACCESS_COMPILE
#ifndef CROSSGEN_COMPILE
//---------------------------------------------------------------------------------------
// 
BOOL LoaderAllocator::CheckAddReference_Unlocked(LoaderAllocator *pOtherLA)
{
    CONTRACTL
    {
        THROWS;
        SO_INTOLERANT;
        MODE_ANY;
    }
    CONTRACTL_END;

    // This must be checked before calling this function
    _ASSERTE(pOtherLA != this);
    
    // This function requires the that loader allocator lock have been taken.
    _ASSERTE(GetDomain()->GetLoaderAllocatorReferencesLock()->OwnedByCurrentThread());
    
    if (m_LoaderAllocatorReferences.Lookup(pOtherLA) == NULL)
    {
        GCX_COOP();
        // Build a managed reference to keep the target object live
        AllocateHandle(pOtherLA->GetExposedObject());

        // Keep track of the references that have already been made
        m_LoaderAllocatorReferences.Add(pOtherLA);

        // Notify the other LoaderAllocator that a reference exists
        pOtherLA->AddReference();
        return TRUE;
    }

    return FALSE;
}

//---------------------------------------------------------------------------------------
// 
BOOL LoaderAllocator::EnsureReference(LoaderAllocator *pOtherLA)
{
    CONTRACTL
    {
        THROWS;
        SO_INTOLERANT;
        MODE_ANY;
    }
    CONTRACTL_END;

    // Check if this lock can be taken in all places that the function is called
    _ASSERTE(GetDomain()->GetLoaderAllocatorReferencesLock()->Debug_CanTake());
    
    if (!IsCollectible())
        return FALSE;

    if (this == pOtherLA)
        return FALSE;

    if (!pOtherLA->IsCollectible())
        return FALSE;

    CrstHolder ch(GetDomain()->GetLoaderAllocatorReferencesLock());
    return CheckAddReference_Unlocked(pOtherLA);
}

BOOL LoaderAllocator::EnsureInstantiation(Module *pDefiningModule, Instantiation inst)
{
    CONTRACTL
    {
        THROWS;
        SO_INTOLERANT;
        MODE_ANY;
    }
    CONTRACTL_END;

    BOOL fNewReferenceNeeded = FALSE;

    // Check if this lock can be taken in all places that the function is called
    _ASSERTE(GetDomain()->GetLoaderAllocatorReferencesLock()->Debug_CanTake());

    if (!IsCollectible())
        return FALSE;

    CrstHolder ch(GetDomain()->GetLoaderAllocatorReferencesLock());

    if (pDefiningModule != NULL)
    {
        LoaderAllocator *pDefiningLoaderAllocator = pDefiningModule->GetLoaderAllocator();
        if (pDefiningLoaderAllocator->IsCollectible())
        {
            if (pDefiningLoaderAllocator != this)
            {
                fNewReferenceNeeded = CheckAddReference_Unlocked(pDefiningLoaderAllocator) || fNewReferenceNeeded;
            }
        }
    }

    for (DWORD i = 0; i < inst.GetNumArgs(); i++)
    {
        TypeHandle arg = inst[i];
        _ASSERTE(!arg.IsEncodedFixup());
        LoaderAllocator *pOtherLA = arg.GetLoaderModule()->GetLoaderAllocator();

        if (pOtherLA == this)
            continue;

        if (!pOtherLA->IsCollectible())
            continue;

        fNewReferenceNeeded = CheckAddReference_Unlocked(pOtherLA) || fNewReferenceNeeded;
    }

    return fNewReferenceNeeded;
}
#else // CROSSGEN_COMPILE
BOOL LoaderAllocator::EnsureReference(LoaderAllocator *pOtherLA)
{
    return FALSE;
}

BOOL LoaderAllocator::EnsureInstantiation(Module *pDefiningModule, Instantiation inst)
{
    return FALSE;
}
#endif // !CROSSGEN_COMPILE

#ifndef CROSSGEN_COMPILE
bool LoaderAllocator::Marked()
{
    LIMITED_METHOD_CONTRACT;
    return m_fMarked;
}

void LoaderAllocator::ClearMark() 
{
    LIMITED_METHOD_CONTRACT; 
    m_fMarked = false;
}

void LoaderAllocator::Mark() 
{
    WRAPPER_NO_CONTRACT;

    if (!m_fMarked) 
    {
        m_fMarked = true;

        LoaderAllocatorSet::Iterator iter = m_LoaderAllocatorReferences.Begin();
        while (iter != m_LoaderAllocatorReferences.End())
        {
            LoaderAllocator *pAllocator = *iter;
            pAllocator->Mark();
            iter++;
        }
    }
}

//---------------------------------------------------------------------------------------
// 
// Collect unreferenced assemblies, remove them from the assembly list and return their loader allocator 
// list.
// 
//static
LoaderAllocator * LoaderAllocator::GCLoaderAllocators_RemoveAssemblies(AppDomain * pAppDomain)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_PREEMPTIVE;
        SO_INTOLERANT;
    }
    CONTRACTL_END;
    // List of LoaderAllocators being deleted
    LoaderAllocator * pFirstDestroyedLoaderAllocator = NULL;
    
#if 0
    // Debug logic for debugging the loader allocator gc.
    {
        /* Iterate through every loader allocator, and print its current state */
        AppDomain::AssemblyIterator iData;
        iData = pAppDomain->IterateAssembliesEx((AssemblyIterationFlags)(
            kIncludeExecution | kIncludeLoaded | kIncludeCollected));
        CollectibleAssemblyHolder<DomainAssembly *> pDomainAssembly;
        
        while (iData.Next_Unlocked(pDomainAssembly.This()))
        {
            // The assembly could be collected (ref-count = 0), do not use holder which calls add-ref
            Assembly * pAssembly = pDomainAssembly->GetLoadedAssembly();

            if (pAssembly != NULL)
            {
                LoaderAllocator * pLoaderAllocator = pAssembly->GetLoaderAllocator();
                if (pLoaderAllocator->IsCollectible())
                {
                    printf("LA %p ReferencesTo %d\n", pLoaderAllocator, pLoaderAllocator->m_cReferences);
                    LoaderAllocatorSet::Iterator iter = pLoaderAllocator->m_LoaderAllocatorReferences.Begin();
                    while (iter != pLoaderAllocator->m_LoaderAllocatorReferences.End())
                    {
                        LoaderAllocator * pAllocator = *iter;
                        printf("LARefTo: %p\n", pAllocator);
                        iter++;
                    }
                }
            }
        }
    }
#endif //0
    
    AppDomain::AssemblyIterator i;
    // Iterate through every loader allocator, marking as we go
    {
        CrstHolder chAssemblyListLock(pAppDomain->GetAssemblyListLock());

        i = pAppDomain->IterateAssembliesEx((AssemblyIterationFlags)(
            kIncludeExecution | kIncludeLoaded | kIncludeCollected));
        CollectibleAssemblyHolder<DomainAssembly *> pDomainAssembly;
        
        while (i.Next_Unlocked(pDomainAssembly.This()))
        {
            // The assembly could be collected (ref-count = 0), do not use holder which calls add-ref
            Assembly * pAssembly = pDomainAssembly->GetLoadedAssembly();
            
            if (pAssembly != NULL)
            {
                LoaderAllocator * pLoaderAllocator = pAssembly->GetLoaderAllocator();
                if (pLoaderAllocator->IsCollectible())
                {
                    if (pLoaderAllocator->IsAlive())
                        pLoaderAllocator->Mark();
                }
            }
        }
    }
    
    // Iterate through every loader allocator, unmarking marked loaderallocators, and
    // build a free list of unmarked ones 
    {
        CrstHolder chLoaderAllocatorReferencesLock(pAppDomain->GetLoaderAllocatorReferencesLock());
        CrstHolder chAssemblyListLock(pAppDomain->GetAssemblyListLock());

        i = pAppDomain->IterateAssembliesEx((AssemblyIterationFlags)(
            kIncludeExecution | kIncludeLoaded | kIncludeCollected));
        CollectibleAssemblyHolder<DomainAssembly *> pDomainAssembly;

        while (i.Next_Unlocked(pDomainAssembly.This()))
        {
            // The assembly could be collected (ref-count = 0), do not use holder which calls add-ref
            Assembly * pAssembly = pDomainAssembly->GetLoadedAssembly();
            
            if (pAssembly != NULL)
            {
                LoaderAllocator * pLoaderAllocator = pAssembly->GetLoaderAllocator();
                if (pLoaderAllocator->IsCollectible())
                {
                    if (pLoaderAllocator->Marked())
                    {
                        pLoaderAllocator->ClearMark();
                    }
                    else if (!pLoaderAllocator->IsAlive())
                    {
                        // Check that we don't have already this LoaderAllocator in the list to destroy
                        // (in case multiple assemblies are loaded in the same LoaderAllocator)
                        bool addAllocator = true;
                        LoaderAllocator * pCheckAllocatorToDestroy = pFirstDestroyedLoaderAllocator;
                        while (pCheckAllocatorToDestroy != NULL)
                        {
                            if (pCheckAllocatorToDestroy == pLoaderAllocator)
                            {
                                addAllocator = false;
                                break;
                            }

                            pCheckAllocatorToDestroy = pCheckAllocatorToDestroy->m_pLoaderAllocatorDestroyNext;
                        }

                        // Otherwise, we have a LoaderAllocator that we add to the list
                        if (addAllocator)
                        {
                            pLoaderAllocator->m_pLoaderAllocatorDestroyNext = pFirstDestroyedLoaderAllocator;
                            // We will store a reference to this assembly, and use it later in this function
                            pFirstDestroyedLoaderAllocator = pLoaderAllocator;
                            _ASSERTE(pLoaderAllocator->m_pFirstDomainAssemblyFromSameALCToDelete != NULL);
                        }
                    }
                }
            }
        }
    }
    
    // Iterate through free list, removing from Assembly list 
    LoaderAllocator * pDomainLoaderAllocatorDestroyIterator = pFirstDestroyedLoaderAllocator;

    while (pDomainLoaderAllocatorDestroyIterator != NULL)
    {
        _ASSERTE(!pDomainLoaderAllocatorDestroyIterator->IsAlive());

        DomainAssemblyIterator domainAssemblyIt(pDomainLoaderAllocatorDestroyIterator->m_pFirstDomainAssemblyFromSameALCToDelete);

        // Release all assemblies from the same ALC
        while (!domainAssemblyIt.end())
        {
            DomainAssembly* domainAssemblyToRemove = domainAssemblyIt;
            pAppDomain->RemoveAssembly(domainAssemblyToRemove);

            if (!domainAssemblyToRemove->GetAssembly()->IsDynamic())
            {
                pAppDomain->RemoveFileFromCache(domainAssemblyToRemove->GetFile());
                AssemblySpec spec;
                spec.InitializeSpec(domainAssemblyToRemove->GetFile());
                VERIFY(pAppDomain->RemoveAssemblyFromCache(domainAssemblyToRemove));
                pAppDomain->RemoveNativeImageDependency(&spec);
            }

            domainAssemblyIt++;
        }

        pDomainLoaderAllocatorDestroyIterator = pDomainLoaderAllocatorDestroyIterator->m_pLoaderAllocatorDestroyNext;
    }
    
    return pFirstDestroyedLoaderAllocator;
} // LoaderAllocator::GCLoaderAllocators_RemoveAssemblies

//---------------------------------------------------------------------------------------
// 
// Collect unreferenced assemblies, delete all their remaining resources.
// 
//static
void LoaderAllocator::GCLoaderAllocators(LoaderAllocator* pOriginalLoaderAllocator)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_PREEMPTIVE;
        SO_INTOLERANT;
    }
    CONTRACTL_END;
    
    // List of LoaderAllocators being deleted
    LoaderAllocator * pFirstDestroyedLoaderAllocator = NULL;
    
    AppDomain* pAppDomain = (AppDomain*)pOriginalLoaderAllocator->GetDomain();

    // Collect all LoaderAllocators that don't have anymore DomainAssemblies alive
    // Note: that it may not collect our pOriginalLoaderAllocator in case this 
    // LoaderAllocator hasn't loaded any DomainAssembly. We handle this case in the next loop.
    // Note: The removed LoaderAllocators are not reachable outside of this function anymore, because we 
    // removed them from the assembly list
    pFirstDestroyedLoaderAllocator = GCLoaderAllocators_RemoveAssemblies(pAppDomain);

    bool isOriginalLoaderAllocatorFound = false;

    // Iterate through free list, firing ETW events and notifying the debugger
    LoaderAllocator * pDomainLoaderAllocatorDestroyIterator = pFirstDestroyedLoaderAllocator;
    while (pDomainLoaderAllocatorDestroyIterator != NULL)
    {
        _ASSERTE(!pDomainLoaderAllocatorDestroyIterator->IsAlive());
        // Fire ETW event
        ETW::LoaderLog::CollectibleLoaderAllocatorUnload((AssemblyLoaderAllocator *)pDomainLoaderAllocatorDestroyIterator);

        // Set the unloaded flag before notifying the debugger
        pDomainLoaderAllocatorDestroyIterator->SetIsUnloaded();

        DomainAssemblyIterator domainAssemblyIt(pDomainLoaderAllocatorDestroyIterator->m_pFirstDomainAssemblyFromSameALCToDelete);
        while (!domainAssemblyIt.end())
        {
            // Notify the debugger
            domainAssemblyIt->NotifyDebuggerUnload();
            domainAssemblyIt++;
        }

        if (pDomainLoaderAllocatorDestroyIterator == pOriginalLoaderAllocator)
        {
            isOriginalLoaderAllocatorFound = true;
        }
        pDomainLoaderAllocatorDestroyIterator = pDomainLoaderAllocatorDestroyIterator->m_pLoaderAllocatorDestroyNext;
    }

    // If the original LoaderAllocator was not processed, it is most likely a LoaderAllocator without any loaded DomainAssembly
    // But we still want to collect it so we add it to the list of LoaderAllocator to destroy
    if (!isOriginalLoaderAllocatorFound && !pOriginalLoaderAllocator->IsAlive())
    {
        pOriginalLoaderAllocator->m_pLoaderAllocatorDestroyNext = pFirstDestroyedLoaderAllocator;
        pFirstDestroyedLoaderAllocator = pOriginalLoaderAllocator;
    }
    
    // Iterate through free list, deleting DomainAssemblies
    pDomainLoaderAllocatorDestroyIterator = pFirstDestroyedLoaderAllocator;
    while (pDomainLoaderAllocatorDestroyIterator != NULL)
    {
        _ASSERTE(!pDomainLoaderAllocatorDestroyIterator->IsAlive());

        DomainAssemblyIterator domainAssemblyIt(pDomainLoaderAllocatorDestroyIterator->m_pFirstDomainAssemblyFromSameALCToDelete);
        while (!domainAssemblyIt.end())
        {
            delete (DomainAssembly*)domainAssemblyIt;
            domainAssemblyIt++;
        }
        // We really don't have to set it to NULL as the assembly is not reachable anymore, but just in case ...
        // (Also debugging NULL AVs if someone uses it accidentally is so much easier)
        pDomainLoaderAllocatorDestroyIterator->m_pFirstDomainAssemblyFromSameALCToDelete = NULL;

        pDomainLoaderAllocatorDestroyIterator->ReleaseManagedAssemblyLoadContext();

        // The following code was previously happening on delete ~DomainAssembly->Terminate
        // We are moving this part here in order to make sure that we can unload a LoaderAllocator
        // that didn't have a DomainAssembly
        // (we have now a LoaderAllocator with 0-n DomainAssembly)

        // This cleanup code starts resembling parts of AppDomain::Terminate too much.
        // It would be useful to reduce duplication and also establish clear responsibilites
        // for LoaderAllocator::Destroy, Assembly::Terminate, LoaderAllocator::Terminate
        // and LoaderAllocator::~LoaderAllocator. We need to establish how these
        // cleanup paths interact with app-domain unload and process tear-down, too.

        if (!IsAtProcessExit())
        {
            // Suspend the EE to do some clean up that can only occur
            // while no threads are running.
            GCX_COOP(); // SuspendEE may require current thread to be in Coop mode
                        // SuspendEE cares about the reason flag only when invoked for a GC
                        // Other values are typically ignored. If using SUSPEND_FOR_APPDOMAIN_SHUTDOWN
                        // is inappropriate, we can introduce a new flag or hijack an unused one.
            ThreadSuspend::SuspendEE(ThreadSuspend::SUSPEND_FOR_APPDOMAIN_SHUTDOWN);
        }

        ExecutionManager::Unload(pDomainLoaderAllocatorDestroyIterator);
        pDomainLoaderAllocatorDestroyIterator->UninitVirtualCallStubManager();

        // TODO: Do we really want to perform this on each LoaderAllocator?
        MethodTable::ClearMethodDataCache();
        ClearJitGenericHandleCache(pAppDomain);

        if (!IsAtProcessExit())
        {
            // Resume the EE.
            ThreadSuspend::RestartEE(FALSE, TRUE);
        }

        // Because RegisterLoaderAllocatorForDeletion is modifying m_pLoaderAllocatorDestroyNext, we are saving it here
        LoaderAllocator* pLoaderAllocatorDestroyNext = pDomainLoaderAllocatorDestroyIterator->m_pLoaderAllocatorDestroyNext;

        // Register this LoaderAllocator for cleanup
        pAppDomain->RegisterLoaderAllocatorForDeletion(pDomainLoaderAllocatorDestroyIterator);

        // Go to next
        pDomainLoaderAllocatorDestroyIterator = pLoaderAllocatorDestroyNext;
    }
    
    // Deleting the DomainAssemblies will have created a list of LoaderAllocator's on the AppDomain
    // Call this shutdown function to clean those up.
    pAppDomain->ShutdownFreeLoaderAllocators(TRUE);
} // LoaderAllocator::GCLoaderAllocators
        
//---------------------------------------------------------------------------------------
// 
//static
BOOL QCALLTYPE LoaderAllocator::Destroy(QCall::LoaderAllocatorHandle pLoaderAllocator)
{
    QCALL_CONTRACT;

    BOOL ret = FALSE;

    BEGIN_QCALL;

    if (ObjectHandleIsNull(pLoaderAllocator->GetLoaderAllocatorObjectHandle()))
    {
        STRESS_LOG1(LF_CLASSLOADER, LL_INFO100, "Begin LoaderAllocator::Destroy for loader allocator %p\n", reinterpret_cast<void *>(static_cast<PTR_LoaderAllocator>(pLoaderAllocator)));
        LoaderAllocatorID *pID = pLoaderAllocator->Id();

        // This will probably change for shared code unloading
        _ASSERTE(pID->GetType() == LAT_Assembly);

        DomainAssembly* pDomainAssembly = (DomainAssembly*)(pID->GetDomainAssemblyIterator());
        if (pDomainAssembly != NULL)
        {
            Assembly *pAssembly = pDomainAssembly->GetCurrentAssembly();

            //if not fully loaded, it is still domain specific, so just get one from DomainAssembly
            BaseDomain *pDomain = pAssembly ? pAssembly->Parent() : pDomainAssembly->GetAppDomain();

            // This will probably change for shared code unloading
            _ASSERTE(pDomain->IsAppDomain());

            AppDomain *pAppDomain = pDomain->AsAppDomain();
            pLoaderAllocator->m_pFirstDomainAssemblyFromSameALCToDelete = pAssembly->GetDomainAssembly(pAppDomain);
        }

        // Iterate through all references to other loader allocators and decrement their reference
        // count
        LoaderAllocatorSet::Iterator iter = pLoaderAllocator->m_LoaderAllocatorReferences.Begin();
        while (iter != pLoaderAllocator->m_LoaderAllocatorReferences.End())
        {
            LoaderAllocator *pAllocator = *iter;
            pAllocator->Release();
            iter++;
        }

        // Release this loader allocator
        BOOL fIsLastReferenceReleased = pLoaderAllocator->Release();

        // If the reference count on this assembly got to 0, then a LoaderAllocator may 
        // be able to be collected, thus, perform a garbage collection.
        // The reference count is setup such that in the case of non-trivial graphs, the reference count
        // may hit zero early.
        if (fIsLastReferenceReleased)
        {
            LoaderAllocator::GCLoaderAllocators(pLoaderAllocator);
        }
        STRESS_LOG1(LF_CLASSLOADER, LL_INFO100, "End LoaderAllocator::Destroy for loader allocator %p\n", reinterpret_cast<void *>(static_cast<PTR_LoaderAllocator>(pLoaderAllocator)));

        ret = TRUE;
    }

    END_QCALL;

    return ret;
} // LoaderAllocator::Destroy

#define MAX_LOADERALLOCATOR_HANDLE 0x40000000

// Returns NULL if the managed LoaderAllocator object was already collected.
LOADERHANDLE LoaderAllocator::AllocateHandle(OBJECTREF value)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_COOPERATIVE;
    }
    CONTRACTL_END;

    LOADERHANDLE retVal;

    struct _gc
    {
        OBJECTREF value;
        LOADERALLOCATORREF loaderAllocator;
        PTRARRAYREF handleTable;
        PTRARRAYREF handleTableOld;
    } gc;

    ZeroMemory(&gc, sizeof(gc));

    GCPROTECT_BEGIN(gc);

    gc.value = value;

    // The handle table is read locklessly, be careful
    if (IsCollectible())
    {
        gc.loaderAllocator = (LOADERALLOCATORREF)ObjectFromHandle(m_hLoaderAllocatorObjectHandle);
        if (gc.loaderAllocator == NULL)
        {   // The managed LoaderAllocator is already collected, we cannot allocate any exposed managed objects for it
            retVal = NULL;
        }
        else
        {
            DWORD slotsUsed;
            DWORD numComponents;

            do
            {
                {
                    CrstHolder ch(&m_crstLoaderAllocator);

                    gc.handleTable = gc.loaderAllocator->GetHandleTable();

                    if (!m_freeHandleIndexesStack.IsEmpty())
                    {
                        // Reuse a handle slot that was previously freed
                        DWORD freeHandleIndex = m_freeHandleIndexesStack.Pop();
                        gc.handleTable->SetAt(freeHandleIndex, gc.value);
                        retVal = (UINT_PTR)((freeHandleIndex + 1) << 1);
                        break;
                    }

                    slotsUsed = gc.loaderAllocator->GetSlotsUsed();

                    if (slotsUsed > MAX_LOADERALLOCATOR_HANDLE)
                    {
                        COMPlusThrowOM();
                    }

                    numComponents = gc.handleTable->GetNumComponents();

                    if (slotsUsed < numComponents)
                    {
                        // The handle table is large enough, allocate next slot from it
                        gc.handleTable->SetAt(slotsUsed, gc.value);
                        gc.loaderAllocator->SetSlotsUsed(slotsUsed + 1);
                        retVal = (UINT_PTR)((slotsUsed + 1) << 1);
                        break;
                    }
                }

                // We need to enlarge the handle table
                gc.handleTableOld = gc.handleTable;

                DWORD newSize = numComponents * 2;
                gc.handleTable = (PTRARRAYREF)AllocateObjectArray(newSize, g_pObjectClass);

                {
                    CrstHolder ch(&m_crstLoaderAllocator);

                    if (gc.loaderAllocator->GetHandleTable() == gc.handleTableOld)
                    {
                        /* Copy out of old array */
                        memmoveGCRefs(gc.handleTable->GetDataPtr(), gc.handleTableOld->GetDataPtr(), slotsUsed * sizeof(Object *));
                        gc.loaderAllocator->SetHandleTable(gc.handleTable);
                    }
                    else
                    {
                        // Another thread has beaten us on enlarging the handle array, use the handle table it has allocated
                        gc.handleTable = gc.loaderAllocator->GetHandleTable();
                    }

                    numComponents = gc.handleTable->GetNumComponents();

                    if (slotsUsed < numComponents)
                    {
                        // The handle table is large enough, allocate next slot from it
                        gc.handleTable->SetAt(slotsUsed, gc.value);
                        gc.loaderAllocator->SetSlotsUsed(slotsUsed + 1);
                        retVal = (UINT_PTR)((slotsUsed + 1) << 1);
                        break;
                    }
                }

                // Loop in the unlikely case that another thread has beaten us on the handle array enlarging, but
                // all the slots were used up before the current thread was scheduled.
            } 
            while (true); 
        }
    }
    else
    {
        OBJECTREF* pRef = GetDomain()->AllocateObjRefPtrsInLargeTable(1);
        SetObjectReference(pRef, gc.value, IsDomainNeutral() ? NULL : GetDomain()->AsAppDomain());
        retVal = (((UINT_PTR)pRef) + 1);
    }

    GCPROTECT_END();

    return retVal;
}

OBJECTREF LoaderAllocator::GetHandleValue(LOADERHANDLE handle)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_COOPERATIVE;
        SO_TOLERANT;
    }
    CONTRACTL_END;

    OBJECTREF objRet = NULL;
    GET_LOADERHANDLE_VALUE_FAST(this, handle, &objRet);
    return objRet;
}

void LoaderAllocator::FreeHandle(LOADERHANDLE handle)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        PRECONDITION(handle != NULL);
    }
    CONTRACTL_END;

    SetHandleValue(handle, NULL);

    if ((((UINT_PTR)handle) & 1) == 0)
    {
        // The slot value doesn't have the low bit set, so it is an index to the handle table.
        // In this case, push the index of the handle to the stack of freed indexes for
        // reuse.
        CrstHolder ch(&m_crstLoaderAllocator);

        UINT_PTR index = (((UINT_PTR)handle) >> 1) - 1;
        // The Push can fail due to OOM. Ignore this failure, it is better than crashing. The
        // only effect is that the slot will not be reused in the future if the runtime survives
        // the low memory situation.
        m_freeHandleIndexesStack.Push((DWORD)index);
    }
}

OBJECTREF LoaderAllocator::CompareExchangeValueInHandle(LOADERHANDLE handle, OBJECTREF valueUNSAFE, OBJECTREF compareUNSAFE)
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_COOPERATIVE;
        PRECONDITION(handle != NULL);
    }
    CONTRACTL_END;

    OBJECTREF retVal;

    struct _gc
    {
        OBJECTREF value;
        OBJECTREF compare;
        OBJECTREF previous;
    } gc;

    ZeroMemory(&gc, sizeof(gc));
    GCPROTECT_BEGIN(gc);

    gc.value = valueUNSAFE;
    gc.compare = compareUNSAFE;

    if ((((UINT_PTR)handle) & 1) != 0)
    {
        OBJECTREF *ptr = (OBJECTREF *)(((UINT_PTR)handle) - 1);
        gc.previous = *ptr;
        if ((*ptr) == gc.compare)
        {
            SetObjectReference(ptr, gc.value, IsDomainNeutral() ? NULL : GetDomain()->AsAppDomain());
        }
    }
    else
    {
        /* The handle table is read locklessly, be careful */
        CrstHolder ch(&m_crstLoaderAllocator);

        _ASSERTE(!ObjectHandleIsNull(m_hLoaderAllocatorObjectHandle));

        UINT_PTR index = (((UINT_PTR)handle) >> 1) - 1;
        LOADERALLOCATORREF loaderAllocator = (LOADERALLOCATORREF)ObjectFromHandle(m_hLoaderAllocatorObjectHandle);
        PTRARRAYREF handleTable = loaderAllocator->GetHandleTable();

        gc.previous = handleTable->GetAt(index);
        if (gc.previous == gc.compare)
        {
            handleTable->SetAt(index, gc.value);
        }
    }

    retVal = gc.previous;
    GCPROTECT_END();

    return retVal;
}

void LoaderAllocator::SetHandleValue(LOADERHANDLE handle, OBJECTREF value)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        PRECONDITION(handle != NULL);
    }
    CONTRACTL_END;

    GCX_COOP();

    GCPROTECT_BEGIN(value);

    // If the slot value does have the low bit set, then it is a simple pointer to the value
    // Otherwise, we will need a more complicated operation to clear the value.
    if ((((UINT_PTR)handle) & 1) != 0)
    {
        OBJECTREF *ptr = (OBJECTREF *)(((UINT_PTR)handle) - 1);
        SetObjectReference(ptr, value, IsDomainNeutral() ? NULL : GetDomain()->AsAppDomain());
    }
    else
    {
        // The handle table is read locklessly, be careful
        CrstHolder ch(&m_crstLoaderAllocator);

        _ASSERTE(!ObjectHandleIsNull(m_hLoaderAllocatorObjectHandle));

        UINT_PTR index = (((UINT_PTR)handle) >> 1) - 1;
        LOADERALLOCATORREF loaderAllocator = (LOADERALLOCATORREF)ObjectFromHandle(m_hLoaderAllocatorObjectHandle);
        PTRARRAYREF handleTable = loaderAllocator->GetHandleTable();
        handleTable->SetAt(index, value);
    }

    GCPROTECT_END();

    return;
}

void LoaderAllocator::SetupManagedTracking(LOADERALLOCATORREF * pKeepLoaderAllocatorAlive)
{
    STANDARD_VM_CONTRACT;

    GCInterface::AddMemoryPressure(30000);
    m_fGCPressure = true;

    GCX_COOP();

    //
    // Initialize managed loader allocator reference holder
    //

    MethodTable *pMT = MscorlibBinder::GetClass(CLASS__LOADERALLOCATOR);

    *pKeepLoaderAllocatorAlive = (LOADERALLOCATORREF)AllocateObject(pMT);

    MethodDescCallSite initLoaderAllocator(METHOD__LOADERALLOCATOR__CTOR, (OBJECTREF *)pKeepLoaderAllocatorAlive);

    ARG_SLOT args[] = {
        ObjToArgSlot(*pKeepLoaderAllocatorAlive)
    };

    initLoaderAllocator.Call(args);

    m_hLoaderAllocatorObjectHandle = GetDomain()->CreateLongWeakHandle(*pKeepLoaderAllocatorAlive);

    RegisterHandleForCleanup(m_hLoaderAllocatorObjectHandle);
}

void LoaderAllocator::ActivateManagedTracking()
{
    CONTRACTL
    {
        NOTHROW;
        GC_TRIGGERS;
        FORBID_FAULT;
        MODE_ANY;
    }
    CONTRACTL_END

    GCX_COOP();

    // There is now one external reference to this LoaderAllocator (the managed scout)
    _ASSERTE(m_cReferences == (UINT32)-1);
    m_cReferences = (UINT32)1;

    LOADERALLOCATORREF loaderAllocator = (LOADERALLOCATORREF)ObjectFromHandle(m_hLoaderAllocatorObjectHandle);
    loaderAllocator->SetNativeLoaderAllocator(this);
}
#endif // !CROSSGEN_COMPILE


// We don't actually allocate a low frequency heap for collectible types
#define COLLECTIBLE_LOW_FREQUENCY_HEAP_SIZE        (0 * GetOsPageSize())
#define COLLECTIBLE_HIGH_FREQUENCY_HEAP_SIZE       (3 * GetOsPageSize())
#define COLLECTIBLE_STUB_HEAP_SIZE                 GetOsPageSize()
#define COLLECTIBLE_CODEHEAP_SIZE                  (7 * GetOsPageSize())
#define COLLECTIBLE_VIRTUALSTUBDISPATCH_HEAP_SPACE (5 * GetOsPageSize())

void LoaderAllocator::Init(BaseDomain *pDomain, BYTE *pExecutableHeapMemory)
{
    STANDARD_VM_CONTRACT;

    m_pDomain = pDomain;

    m_crstLoaderAllocator.Init(CrstLoaderAllocator, (CrstFlags)CRST_UNSAFE_COOPGC);

    //
    // Initialize the heaps
    //

    DWORD dwLowFrequencyHeapReserveSize;
    DWORD dwHighFrequencyHeapReserveSize;
    DWORD dwStubHeapReserveSize;
    DWORD dwExecutableHeapReserveSize;
    DWORD dwCodeHeapReserveSize;
    DWORD dwVSDHeapReserveSize;

    dwExecutableHeapReserveSize = 0;

    if (IsCollectible())
    {
        dwLowFrequencyHeapReserveSize  = COLLECTIBLE_LOW_FREQUENCY_HEAP_SIZE;
        dwHighFrequencyHeapReserveSize = COLLECTIBLE_HIGH_FREQUENCY_HEAP_SIZE;
        dwStubHeapReserveSize          = COLLECTIBLE_STUB_HEAP_SIZE;
        dwCodeHeapReserveSize          = COLLECTIBLE_CODEHEAP_SIZE;
        dwVSDHeapReserveSize           = COLLECTIBLE_VIRTUALSTUBDISPATCH_HEAP_SPACE;
    }
    else
    {
        dwLowFrequencyHeapReserveSize  = LOW_FREQUENCY_HEAP_RESERVE_SIZE;
        dwHighFrequencyHeapReserveSize = HIGH_FREQUENCY_HEAP_RESERVE_SIZE;
        dwStubHeapReserveSize          = STUB_HEAP_RESERVE_SIZE;

        // Non-collectible assemblies do not reserve space for these heaps.
        dwCodeHeapReserveSize = 0;
        dwVSDHeapReserveSize = 0;
    }

    // The global heap needs a bit of space for executable memory that is not associated with a rangelist.
    // Take a page from the high-frequency heap for this.
    if (pExecutableHeapMemory != NULL)
    {
#ifdef FEATURE_WINDOWSPHONE
        // code:UMEntryThunk::CreateUMEntryThunk allocates memory on executable loader heap for phone.
        // Reserve enough for a typical phone app to fit.
        dwExecutableHeapReserveSize = 3 * GetOsPageSize();
#else
        dwExecutableHeapReserveSize = GetOsPageSize();
#endif

        _ASSERTE(dwExecutableHeapReserveSize < dwHighFrequencyHeapReserveSize);
        dwHighFrequencyHeapReserveSize -= dwExecutableHeapReserveSize;
    }

    DWORD dwTotalReserveMemSize = dwLowFrequencyHeapReserveSize
                                + dwHighFrequencyHeapReserveSize
                                + dwStubHeapReserveSize
                                + dwCodeHeapReserveSize
                                + dwVSDHeapReserveSize
                                + dwExecutableHeapReserveSize;

    dwTotalReserveMemSize = (DWORD) ALIGN_UP(dwTotalReserveMemSize, VIRTUAL_ALLOC_RESERVE_GRANULARITY);

#if !defined(_WIN64)
    // Make sure that we reserve as little as possible on 32-bit to save address space
    _ASSERTE(dwTotalReserveMemSize <= VIRTUAL_ALLOC_RESERVE_GRANULARITY);
#endif

    BYTE * initReservedMem = ClrVirtualAllocExecutable(dwTotalReserveMemSize, MEM_RESERVE, PAGE_NOACCESS);

    m_InitialReservedMemForLoaderHeaps = initReservedMem;

    if (initReservedMem == NULL)
        COMPlusThrowOM();

    if (IsCollectible())
    {
        m_pCodeHeapInitialAlloc = initReservedMem;
        initReservedMem += dwCodeHeapReserveSize;
        m_pVSDHeapInitialAlloc = initReservedMem;
        initReservedMem += dwVSDHeapReserveSize;
    }
    else
    {
        _ASSERTE((dwCodeHeapReserveSize == 0) && (m_pCodeHeapInitialAlloc == NULL));
        _ASSERTE((dwVSDHeapReserveSize == 0) && (m_pVSDHeapInitialAlloc == NULL));
    }

    if (dwLowFrequencyHeapReserveSize != 0)
    {
        _ASSERTE(!IsCollectible());

        m_pLowFrequencyHeap = new (&m_LowFreqHeapInstance) LoaderHeap(LOW_FREQUENCY_HEAP_RESERVE_SIZE,
                                                                      LOW_FREQUENCY_HEAP_COMMIT_SIZE,
                                                                      initReservedMem,
                                                                      dwLowFrequencyHeapReserveSize,
                                                                      LOADERHEAP_PROFILE_COUNTER);
        initReservedMem += dwLowFrequencyHeapReserveSize;
    }

    if (dwExecutableHeapReserveSize != 0)
    {
        _ASSERTE(!IsCollectible());

        m_pExecutableHeap = new (pExecutableHeapMemory) LoaderHeap(STUB_HEAP_RESERVE_SIZE,
                                                                      STUB_HEAP_COMMIT_SIZE,
                                                                      initReservedMem,
                                                                      dwExecutableHeapReserveSize,
                                                                      LOADERHEAP_PROFILE_COUNTER,
                                                                      NULL,
                                                                      TRUE /* Make heap executable */
                                                                      );
        initReservedMem += dwExecutableHeapReserveSize;
    }

    m_pHighFrequencyHeap = new (&m_HighFreqHeapInstance) LoaderHeap(HIGH_FREQUENCY_HEAP_RESERVE_SIZE,
                                                                    HIGH_FREQUENCY_HEAP_COMMIT_SIZE,
                                                                    initReservedMem,
                                                                    dwHighFrequencyHeapReserveSize,
                                                                    LOADERHEAP_PROFILE_COUNTER);
    initReservedMem += dwHighFrequencyHeapReserveSize;

    if (IsCollectible())
        m_pLowFrequencyHeap = m_pHighFrequencyHeap;

#if defined(_DEBUG) && defined(STUBLINKER_GENERATES_UNWIND_INFO)
    m_pHighFrequencyHeap->m_fPermitStubsWithUnwindInfo = TRUE;
#endif

    m_pStubHeap = new (&m_StubHeapInstance) LoaderHeap(STUB_HEAP_RESERVE_SIZE,
                                                       STUB_HEAP_COMMIT_SIZE,
                                                       initReservedMem,
                                                       dwStubHeapReserveSize,
                                                       LOADERHEAP_PROFILE_COUNTER,
                                                       STUBMANAGER_RANGELIST(StubLinkStubManager),
                                                       TRUE /* Make heap executable */);

    initReservedMem += dwStubHeapReserveSize;

#if defined(_DEBUG) && defined(STUBLINKER_GENERATES_UNWIND_INFO)
    m_pStubHeap->m_fPermitStubsWithUnwindInfo = TRUE;
#endif

#ifdef CROSSGEN_COMPILE
    m_pPrecodeHeap = new (&m_PrecodeHeapInstance) LoaderHeap(GetOsPageSize(), GetOsPageSize());
#else
    m_pPrecodeHeap = new (&m_PrecodeHeapInstance) CodeFragmentHeap(this, STUB_CODE_BLOCK_PRECODE);
#endif
}


#ifndef CROSSGEN_COMPILE

#ifdef FEATURE_READYTORUN
PTR_CodeFragmentHeap LoaderAllocator::GetDynamicHelpersHeap()
{
    CONTRACTL {
        THROWS;
        MODE_ANY;
    } CONTRACTL_END;

    if (m_pDynamicHelpersHeap == NULL)
    {
        CodeFragmentHeap * pDynamicHelpersHeap = new CodeFragmentHeap(this, STUB_CODE_BLOCK_DYNAMICHELPER);
        if (InterlockedCompareExchangeT(&m_pDynamicHelpersHeap, pDynamicHelpersHeap, NULL) != NULL)
            delete pDynamicHelpersHeap;
    }
    return m_pDynamicHelpersHeap;
}
#endif

FuncPtrStubs * LoaderAllocator::GetFuncPtrStubs()
{
    CONTRACTL {
        THROWS;
        MODE_ANY;
    } CONTRACTL_END;

    if (m_pFuncPtrStubs == NULL)
    {
        FuncPtrStubs * pFuncPtrStubs = new FuncPtrStubs();
        if (InterlockedCompareExchangeT(&m_pFuncPtrStubs, pFuncPtrStubs, NULL) != NULL)
            delete pFuncPtrStubs;
    }
    return m_pFuncPtrStubs;
}

BYTE *LoaderAllocator::GetVSDHeapInitialBlock(DWORD *pSize)
{
    LIMITED_METHOD_CONTRACT;

    *pSize = 0;
    BYTE *buffer = InterlockedCompareExchangeT(&m_pVSDHeapInitialAlloc, NULL, m_pVSDHeapInitialAlloc);
    if (buffer != NULL)
    {
        *pSize = COLLECTIBLE_VIRTUALSTUBDISPATCH_HEAP_SPACE;
    }
    return buffer;
}

BYTE *LoaderAllocator::GetCodeHeapInitialBlock(const BYTE * loAddr, const BYTE * hiAddr, DWORD minimumSize, DWORD *pSize)
{
    LIMITED_METHOD_CONTRACT;

    *pSize = 0;
    // Check to see if the size is small enough that this might work
    if (minimumSize > COLLECTIBLE_CODEHEAP_SIZE)
        return NULL;

    // Check to see if initial alloc would be in the proper region
    if (loAddr != NULL || hiAddr != NULL)
    {
        if (m_pCodeHeapInitialAlloc < loAddr)
            return NULL;
        if ((m_pCodeHeapInitialAlloc + COLLECTIBLE_CODEHEAP_SIZE) > hiAddr)
            return NULL;
    }

    BYTE * buffer = InterlockedCompareExchangeT(&m_pCodeHeapInitialAlloc, NULL, m_pCodeHeapInitialAlloc);
    if (buffer != NULL)
    {
        *pSize = COLLECTIBLE_CODEHEAP_SIZE;
    }
    return buffer;
}

// in retail should be called from AppDomain::Terminate
void LoaderAllocator::Terminate()
{
    CONTRACTL {
        NOTHROW;
        GC_TRIGGERS;
        MODE_ANY;
        SO_INTOLERANT;
    } CONTRACTL_END;

    if (m_fTerminated)
        return;

    m_fTerminated = true;

    LOG((LF_CLASSLOADER, LL_INFO100, "Begin LoaderAllocator::Terminate for loader allocator %p\n", reinterpret_cast<void *>(static_cast<PTR_LoaderAllocator>(this))));

    if (m_fGCPressure)
    {
        GCX_PREEMP();
        GCInterface::RemoveMemoryPressure(30000);
        m_fGCPressure = false;
    }

    delete m_pUMEntryThunkCache;
    m_pUMEntryThunkCache = NULL;

    m_crstLoaderAllocator.Destroy();
    m_LoaderAllocatorReferences.RemoveAll();

    // In collectible types we merge the low frequency and high frequency heaps
    // So don't destroy them twice.
    if ((m_pLowFrequencyHeap != NULL) && (m_pLowFrequencyHeap != m_pHighFrequencyHeap))
    {
        m_pLowFrequencyHeap->~LoaderHeap();
        m_pLowFrequencyHeap = NULL;
    }

    if (m_pHighFrequencyHeap != NULL)
    {
#ifdef STUBLINKER_GENERATES_UNWIND_INFO
        UnregisterUnwindInfoInLoaderHeap(m_pHighFrequencyHeap);
#endif

        m_pHighFrequencyHeap->~LoaderHeap();
        m_pHighFrequencyHeap = NULL;
    }

    if (m_pStubHeap != NULL)
    {
#ifdef STUBLINKER_GENERATES_UNWIND_INFO
        UnregisterUnwindInfoInLoaderHeap(m_pStubHeap);
#endif

        m_pStubHeap->~LoaderHeap();
        m_pStubHeap = NULL;
    }

    if (m_pPrecodeHeap != NULL)
    {
        m_pPrecodeHeap->~CodeFragmentHeap();
        m_pPrecodeHeap = NULL;
    }

#ifdef FEATURE_READYTORUN
    if (m_pDynamicHelpersHeap != NULL)
    {
        delete m_pDynamicHelpersHeap;
        m_pDynamicHelpersHeap = NULL;
    }
#endif

    if (m_pFuncPtrStubs != NULL)
    {
        delete m_pFuncPtrStubs;
        m_pFuncPtrStubs = NULL;
    }

    // This was the block reserved by BaseDomain::Init for the loaderheaps.
    if (m_InitialReservedMemForLoaderHeaps)
    {
        ClrVirtualFree (m_InitialReservedMemForLoaderHeaps, 0, MEM_RELEASE);
        m_InitialReservedMemForLoaderHeaps=NULL;
    }

#ifdef FAT_DISPATCH_TOKENS
    if (m_pFatTokenSetLock != NULL)
    {
        delete m_pFatTokenSetLock;
        m_pFatTokenSetLock = NULL;
    }

    if (m_pFatTokenSet != NULL)
    {
        delete m_pFatTokenSet;
        m_pFatTokenSet = NULL;
    }
#endif // FAT_DISPATCH_TOKENS

    CleanupStringLiteralMap();

    LOG((LF_CLASSLOADER, LL_INFO100, "End LoaderAllocator::Terminate for loader allocator %p\n", reinterpret_cast<void *>(static_cast<PTR_LoaderAllocator>(this))));
}

#endif // !CROSSGEN_COMPILE


#else //DACCESS_COMPILE
void LoaderAllocator::EnumMemoryRegions(CLRDataEnumMemoryFlags flags)
{
    SUPPORTS_DAC;
    DAC_ENUM_DTHIS();
    if (m_pLowFrequencyHeap.IsValid())
    {
        m_pLowFrequencyHeap->EnumMemoryRegions(flags);
    }
    if (m_pHighFrequencyHeap.IsValid())
    {
        m_pHighFrequencyHeap->EnumMemoryRegions(flags);
    }
    if (m_pStubHeap.IsValid())
    {
        m_pStubHeap->EnumMemoryRegions(flags);
    }
    if (m_pPrecodeHeap.IsValid())
    {
        m_pPrecodeHeap->EnumMemoryRegions(flags);
    }
    if (m_pPrecodeHeap.IsValid())
    {
        m_pPrecodeHeap->EnumMemoryRegions(flags);
    }
}
#endif //DACCESS_COMPILE

SIZE_T LoaderAllocator::EstimateSize()
{
    WRAPPER_NO_CONTRACT;
    SIZE_T retval=0;
    if(m_pHighFrequencyHeap) 
        retval+=m_pHighFrequencyHeap->GetSize();
    if(m_pLowFrequencyHeap) 
        retval+=m_pLowFrequencyHeap->GetSize();  
    if(m_pStubHeap) 
        retval+=m_pStubHeap->GetSize();   
    if(m_pStringLiteralMap)
        retval+=m_pStringLiteralMap->GetSize();
#ifndef CROSSGEN_COMPILE
    if(m_pVirtualCallStubManager)
        retval+=m_pVirtualCallStubManager->GetSize();
#endif

    return retval;    
}

#ifndef DACCESS_COMPILE

#ifndef CROSSGEN_COMPILE

DispatchToken LoaderAllocator::GetDispatchToken(
    UINT32 typeId, UINT32 slotNumber)
{
    CONTRACTL {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        INJECT_FAULT(COMPlusThrowOM(););
    } CONTRACTL_END;

#ifdef FAT_DISPATCH_TOKENS

    if (DispatchToken::RequiresDispatchTokenFat(typeId, slotNumber))
    {
        //
        // Lock and set are lazily created.
        //
        if (m_pFatTokenSetLock == NULL)
        {
            NewHolder<SimpleRWLock> pFatTokenSetLock = new SimpleRWLock(COOPERATIVE_OR_PREEMPTIVE, LOCK_TYPE_DEFAULT);
            SimpleWriteLockHolder lock(pFatTokenSetLock);
            NewHolder<FatTokenSet> pFatTokenSet = new FatTokenSet;

            if (FastInterlockCompareExchangePointer(
                    &m_pFatTokenSetLock, pFatTokenSetLock.GetValue(), NULL) != NULL)
            {   // Someone beat us to it
                lock.Release();
                // NewHolder will delete lock.
            }
            else
            {   // Make sure second allocation succeeds before suppressing holder of first.
                pFatTokenSetLock.SuppressRelease();
                m_pFatTokenSet = pFatTokenSet;
                pFatTokenSet.SuppressRelease();
            }
        }

        //
        // Take read lock, see if the requisite token has already been created and if so use it.
        // Otherwise, take write lock and create new token and add to the set.
        //

        // Lookup
        SimpleReadLockHolder rlock(m_pFatTokenSetLock);
        DispatchTokenFat key(typeId, slotNumber);
        DispatchTokenFat *pFat = m_pFatTokenSet->Lookup(&key);
        if (pFat != NULL)
        {   // <typeId,slotNumber> is already in the set.
            return DispatchToken(pFat);
        }
        else
        {   // Create
            rlock.Release();
            SimpleWriteLockHolder wlock(m_pFatTokenSetLock);

            // Check to see if someone beat us to the punch between
            // releasing the read lock and taking the write lock.
            pFat = m_pFatTokenSet->Lookup(&key);

            if (pFat == NULL)
            {   // No one beat us; allocate and insert a new DispatchTokenFat instance.
                pFat = new ((LPVOID)GetHighFrequencyHeap()->AllocMem(S_SIZE_T(sizeof(DispatchTokenFat))))
                    DispatchTokenFat(typeId, slotNumber);

                m_pFatTokenSet->Add(pFat);
            }

            return DispatchToken(pFat);
        }
    }
#endif // FAT_DISPATCH_TOKENS

    return DispatchToken::CreateDispatchToken(typeId, slotNumber);
}

DispatchToken LoaderAllocator::TryLookupDispatchToken(UINT32 typeId, UINT32 slotNumber)
{
    CONTRACTL {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        SO_TOLERANT;
    } CONTRACTL_END;

#ifdef FAT_DISPATCH_TOKENS

    if (DispatchToken::RequiresDispatchTokenFat(typeId, slotNumber))
    {
        if (m_pFatTokenSetLock != NULL)
        {
            DispatchTokenFat * pFat = NULL;
            // Stack probes and locking operations are throwing. Catch all
            // exceptions and just return an invalid token, since this is
            EX_TRY
            {
                BEGIN_SO_INTOLERANT_CODE(GetThread());
                SimpleReadLockHolder rlock(m_pFatTokenSetLock);
                if (m_pFatTokenSet != NULL)
                {
                    DispatchTokenFat key(typeId, slotNumber);
                    pFat = m_pFatTokenSet->Lookup(&key);
                }
                END_SO_INTOLERANT_CODE;
            }
            EX_CATCH
            {
                pFat = NULL;
            }
            EX_END_CATCH(SwallowAllExceptions);

            if (pFat != NULL)
            {
                return DispatchToken(pFat);
            }
        }
        // Return invalid token when not found.
        return DispatchToken();
    }
    else
#endif // FAT_DISPATCH_TOKENS
    {
        return DispatchToken::CreateDispatchToken(typeId, slotNumber);
    }
}

void LoaderAllocator::InitVirtualCallStubManager(BaseDomain * pDomain)
{
    STANDARD_VM_CONTRACT;

    NewHolder<VirtualCallStubManager> pMgr(new VirtualCallStubManager());

    // Init the manager, including all heaps and such.
    pMgr->Init(pDomain, this);

    m_pVirtualCallStubManager = pMgr;

    // Successfully created the manager.
    pMgr.SuppressRelease();
}

void LoaderAllocator::UninitVirtualCallStubManager()
{    
    WRAPPER_NO_CONTRACT;

    if (m_pVirtualCallStubManager != NULL)
    {
        m_pVirtualCallStubManager->Uninit();
        delete m_pVirtualCallStubManager;
        m_pVirtualCallStubManager = NULL;
    }
}
#endif // !CROSSGEN_COMPILE

#endif // !DACCESS_COMPILE

BOOL GlobalLoaderAllocator::CanUnload()
{
    LIMITED_METHOD_CONTRACT;

    return FALSE;
}

BOOL AppDomainLoaderAllocator::CanUnload()
{
    CONTRACTL {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        SO_TOLERANT;
    } CONTRACTL_END;

    return FALSE;
}

BOOL AssemblyLoaderAllocator::CanUnload()
{
    LIMITED_METHOD_CONTRACT;

    return TRUE;
}

BOOL LoaderAllocator::IsDomainNeutral()
{
    CONTRACTL {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        SO_TOLERANT;
    } CONTRACTL_END;

    return GetDomain()->IsSharedDomain();
}

DomainAssemblyIterator::DomainAssemblyIterator(DomainAssembly* pFirstAssembly)
{
    pCurrentAssembly = pFirstAssembly;
    pNextAssembly = pCurrentAssembly ? pCurrentAssembly->GetNextDomainAssemblyInSameALC() : NULL;
}

void DomainAssemblyIterator::operator++()
{
    pCurrentAssembly = pNextAssembly;
    pNextAssembly = pCurrentAssembly ? pCurrentAssembly->GetNextDomainAssemblyInSameALC() : NULL;
}

void AssemblyLoaderAllocator::SetCollectible()
{
    CONTRACTL
    {
        THROWS;
    }
    CONTRACTL_END;

    m_IsCollectible = true;
#ifndef DACCESS_COMPILE
    m_pShuffleThunkCache = new ShuffleThunkCache(m_pStubHeap);
#endif
}

#ifndef DACCESS_COMPILE

#ifndef CROSSGEN_COMPILE

AssemblyLoaderAllocator::~AssemblyLoaderAllocator()
{
    if (m_binderToRelease != NULL)
    {
        VERIFY(m_binderToRelease->Release() == 0);
        m_binderToRelease = NULL;
    }

    delete m_pShuffleThunkCache;
    m_pShuffleThunkCache = NULL;
}

void AssemblyLoaderAllocator::RegisterBinder(CLRPrivBinderAssemblyLoadContext* binderToRelease)
{
    // When the binder is registered it will be released by the destructor
    // of this instance
    _ASSERTE(m_binderToRelease == NULL);
    m_binderToRelease = binderToRelease;
}

STRINGREF *LoaderAllocator::GetStringObjRefPtrFromUnicodeString(EEStringData *pStringData)
{
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_COOPERATIVE;
        PRECONDITION(CheckPointer(pStringData));
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;
    if (m_pStringLiteralMap == NULL)
    {
        LazyInitStringLiteralMap();
    }
    _ASSERTE(m_pStringLiteralMap);
    return m_pStringLiteralMap->GetStringLiteral(pStringData, TRUE, !CanUnload());
}

//*****************************************************************************
void LoaderAllocator::LazyInitStringLiteralMap()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;

    NewHolder<StringLiteralMap> pStringLiteralMap(new StringLiteralMap());

    pStringLiteralMap->Init();

    if (InterlockedCompareExchangeT<StringLiteralMap *>(&m_pStringLiteralMap, pStringLiteralMap, NULL) == NULL)
    {
        pStringLiteralMap.SuppressRelease();
    }
}

void LoaderAllocator::CleanupStringLiteralMap()
{
    CONTRACTL
    {
        NOTHROW;
        GC_TRIGGERS;
        MODE_ANY;
    }
    CONTRACTL_END;

    if (m_pStringLiteralMap)
    {
        delete m_pStringLiteralMap;
        m_pStringLiteralMap = NULL;
    }
}

STRINGREF *LoaderAllocator::IsStringInterned(STRINGREF *pString)
{
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_COOPERATIVE;
        PRECONDITION(CheckPointer(pString));
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;
    if (m_pStringLiteralMap == NULL)
    {
        LazyInitStringLiteralMap();
    }
    _ASSERTE(m_pStringLiteralMap);
    return m_pStringLiteralMap->GetInternedString(pString, FALSE, !CanUnload());
}

STRINGREF *LoaderAllocator::GetOrInternString(STRINGREF *pString)
{
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_COOPERATIVE;
        PRECONDITION(CheckPointer(pString));
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;
    if (m_pStringLiteralMap == NULL)
    {
        LazyInitStringLiteralMap();
    }
    _ASSERTE(m_pStringLiteralMap);
    return m_pStringLiteralMap->GetInternedString(pString, TRUE, !CanUnload());
}

void AssemblyLoaderAllocator::RegisterHandleForCleanup(OBJECTHANDLE objHandle)
{
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_ANY;
        CAN_TAKE_LOCK;
        PRECONDITION(CheckPointer(objHandle));
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;

    void * pItem = GetLowFrequencyHeap()->AllocMem(S_SIZE_T(sizeof(HandleCleanupListItem)));

    // InsertTail must be protected by a lock. Just use the loader allocator lock
    CrstHolder ch(&m_crstLoaderAllocator);
    m_handleCleanupList.InsertTail(new (pItem) HandleCleanupListItem(objHandle));
}

void AssemblyLoaderAllocator::CleanupHandles()
{
    CONTRACTL
    {
        GC_TRIGGERS;
        NOTHROW;
        MODE_ANY;
        CAN_TAKE_LOCK;
    }
    CONTRACTL_END;

    _ASSERTE(GetDomain()->IsAppDomain());

    // This method doesn't take a lock around RemoveHead because it's supposed to
    // be called only from Terminate
    while (!m_handleCleanupList.IsEmpty())
    {
        HandleCleanupListItem * pItem = m_handleCleanupList.RemoveHead();
        DestroyTypedHandle(pItem->m_handle);
    }
}

void LoaderAllocator::RegisterFailedTypeInitForCleanup(ListLockEntry *pListLockEntry)
{    
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_ANY;
        CAN_TAKE_LOCK;
        PRECONDITION(CheckPointer(pListLockEntry));
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;

    if (!IsCollectible())
    {
        return;
    }

    void * pItem = GetLowFrequencyHeap()->AllocMem(S_SIZE_T(sizeof(FailedTypeInitCleanupListItem)));

    // InsertTail must be protected by a lock. Just use the loader allocator lock
    CrstHolder ch(&m_crstLoaderAllocator);
    m_failedTypeInitCleanupList.InsertTail(new (pItem) FailedTypeInitCleanupListItem(pListLockEntry));
}

void LoaderAllocator::CleanupFailedTypeInit()
{
    CONTRACTL
    {
        GC_TRIGGERS;
        THROWS;
        MODE_ANY;
        CAN_TAKE_LOCK;
    }
    CONTRACTL_END;

    if (!IsCollectible())
    {
        return;
    }

    _ASSERTE(GetDomain()->IsAppDomain());

    // This method doesn't take a lock around loader allocator state access, because
    // it's supposed to be called only during cleanup. However, the domain-level state
    // might be accessed by multiple threads.
    ListLock *pLock = GetDomain()->GetClassInitLock();

    while (!m_failedTypeInitCleanupList.IsEmpty())
    {
        FailedTypeInitCleanupListItem * pItem = m_failedTypeInitCleanupList.RemoveHead();

        ListLockHolder pInitLock(pLock);
        pLock->Unlink(pItem->m_pListLockEntry);
    }
}

void AssemblyLoaderAllocator::ReleaseManagedAssemblyLoadContext()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        SO_INTOLERANT;
    }
    CONTRACTL_END;

    if (m_binderToRelease != NULL)
    {
        // Release the managed ALC
        m_binderToRelease->ReleaseLoadContext();
    }
}

// U->M thunks created in this LoaderAllocator and not associated with a delegate.
UMEntryThunkCache *LoaderAllocator::GetUMEntryThunkCache()
{
    CONTRACTL
    {
        THROWS;
        GC_TRIGGERS;
        MODE_ANY;
        INJECT_FAULT(COMPlusThrowOM(););
    }
    CONTRACTL_END;

    if (!m_pUMEntryThunkCache)
    {
        UMEntryThunkCache *pUMEntryThunkCache = new UMEntryThunkCache(GetAppDomain());

        if (FastInterlockCompareExchangePointer(&m_pUMEntryThunkCache, pUMEntryThunkCache, NULL) != NULL)
        {
            // some thread swooped in and set the field
            delete pUMEntryThunkCache;
        }
    }
    _ASSERTE(m_pUMEntryThunkCache);
    return m_pUMEntryThunkCache;
}

#endif // !CROSSGEN_COMPILE

MethodTable *LoaderAllocator::DerivedMethodTableTraits::GetKey(MethodTable *mt) 
{
    LIMITED_METHOD_DAC_CONTRACT; 
    return mt->GetParentMethodTable();
}

bool LoaderAllocator::DependsOnLoaderAllocator(LoaderAllocator* pLoaderAllocator)
{
    STANDARD_VM_CONTRACT;

    if (pLoaderAllocator == this)
        return true;

    LoaderAllocatorSet laVisited;

    CrstHolder ch(GetDomain()->GetLoaderAllocatorReferencesLock());
    laVisited.Add(this);
    return DependsOnLoaderAllocator_Worker(this, pLoaderAllocator, laVisited);
}

/*static*/
bool LoaderAllocator::DependsOnLoaderAllocator_Worker(LoaderAllocator* pLASearchOn, LoaderAllocator* pLASearchFor, LoaderAllocatorSet &laVisited)
{
    STANDARD_VM_CONTRACT;

    LoaderAllocatorSet::Iterator iter = pLASearchOn->m_LoaderAllocatorReferences.Begin();
    while (iter != pLASearchOn->m_LoaderAllocatorReferences.End())
    {
        LoaderAllocator *pAllocator = *iter;

        if (pLASearchFor == pAllocator)
            return true;

        if (laVisited.Lookup(pAllocator) != NULL)
        {
            // Already visited this allocator
            continue;
        }

        laVisited.Add(pAllocator);
        if (DependsOnLoaderAllocator_Worker(pAllocator, pLASearchFor, laVisited))
            return true;

        iter++;
    }

    return false;
}


bool LoaderAllocator::MTHasDerivedType(MethodTable *pMT)
{
    WRAPPER_NO_CONTRACT;
    return pMT->HasDerivedType();
}

bool LoaderAllocator::MTHasDerivedTypeInOtherLoaderAllocator(MethodTable *pMT)
{
    WRAPPER_NO_CONTRACT;
    return pMT->HasDerivedTypeInOtherLoaderAllocator();
}

LoaderAllocator* LoaderAllocator::MTGetLoaderAllocator(MethodTable *pMT)
{
    WRAPPER_NO_CONTRACT;
    return pMT->GetLoaderAllocator();
}
#endif // !DACCESS_COMPILE

void LoaderAllocator::AddDerivedTypeInfo(MethodTable *pBaseType, MethodTable *pDerivedOrImplementingType)
{
    STANDARD_VM_CONTRACT;

#ifndef DACCESS_COMPILE
    CrstHolder ch(&m_crstLoaderAllocator);
    _ASSERTE(!pDerivedOrImplementingType->IsInterface());
    _ASSERTE(pDerivedOrImplementingType->GetLoaderAllocator() == this);

    if (!pBaseType->IsInterface())
    {
        _ASSERTE(pDerivedOrImplementingType->GetParentMethodTable() == pBaseType);
        m_derivedTypes.Add(pDerivedOrImplementingType);
    }
    else
    {
        InterfaceTypeToImplementingTypeEntry entry;
        entry.m_pInterfaceType = pBaseType;
        entry.m_pImplementingType = pDerivedOrImplementingType;
        m_interfaceImplementations.Add(entry);
    }
#endif // DACCESS_COMPILE
}

#ifndef DACCESS_COMPILE

bool LoaderAllocator::MayInsertReferenceToTypeHandleInCode(TypeHandle th)
{
    STANDARD_VM_CONTRACT;

    LoaderAllocator *laTargetType = th.GetLoaderAllocator();
    return DependsOnLoaderAllocator(laTargetType);
}

// This function is designed to return false if there are any derived types which implement a specific vtable slot
// There may be multiple derived types which use the same slot implementation
// pCheckableCondition will be changed to report a flag to indicate when this property changes.
bool LoaderAllocator::DoesAnyTypeOverrideVTableSlot(MethodTable *pMT, DWORD slot, CheckableConditionForOptimizationChange* pCheckableCondition)
{
    STANDARD_VM_CONTRACT;
    _ASSERTE(pMT->GetLoaderAllocator() == this);

    if (pCheckableCondition != nullptr)
        *pCheckableCondition = pMT->GenerateCheckableConditionForMethodSlot(slot);

    if (!pMT->HasDerivedType())
        return false;

    MethodDesc *pMDForSlot = pMT->GetMethodDescForSlot(slot);
    bool reportValue = false;

    reportValue = !WalkDerivingAndImplementingMethodTables(pMT, [slot, pMDForSlot](MethodTable *pDerivedMethodTable)
    {
        if (pDerivedMethodTable->GetMethodDescForSlot(slot) != pMDForSlot)
        {
            return false;
        }
        return true;
    });

    return reportValue;
}

// This function will report a unique methodtable which is type or derives from type. 
// If the type is non-abstract, it will report null if there are any derived types.
// If the type is abstract, it will return null if there are more than one derived types which are concrete types, or if
// there is one derived concrete type, it will return that one.
// This function may return null in cases where only one type would actually satisfy the condition in the case of collectible types
// pCheckableCondition will be changed to report a flag to indicate when this property changes.
MethodTable* LoaderAllocator::FindUniqueConcreteTypeWhichWithTypeInTypeHierarchy(MethodTable *pMT, CheckableConditionForOptimizationChange* pCheckableCondition)
{
    STANDARD_VM_CONTRACT;

    _ASSERTE(pMT->GetLoaderAllocator() == this);

    if (pCheckableCondition != nullptr)
        *pCheckableCondition = pMT->GenerateCheckableConditionForTypeDerivationChange();

    if (!pMT->IsAbstract())
    {
        if (pMT->HasDerivedType())
        {
            // Technically not quite right. There could exist derived abstract types, but that is an unlikely scenario.
            return nullptr;
        }
        else
        {
            return pMT;
        }
    }
    else
    {
        // Walk derived types, and stop walking and report null if multiple concrete derived types are found.
        MethodTable *pUniqueType = nullptr;
        if (!WalkDerivingAndImplementingMethodTables(pMT, [&pUniqueType](MethodTable *pDerivedMethodTable)
        {
            if (!pDerivedMethodTable->IsAbstract())
            {
                if (pUniqueType != nullptr)
                {
                    pUniqueType = nullptr;
                    return false;
                }
                pUniqueType = pDerivedMethodTable;
            }

            return true;
        }))
        {
            return nullptr;
        }
        return pUniqueType;
    }
}

// This function will report a unique methodtable which implements the type. 
// It will return null if there are more than one derived types which are concrete types, or if
// there is one derived concrete type, it will return that one.
// This function may return null in cases where only one type would actually satisfy the condition in the case of collectible types
// pCheckableCondition will be changed to report a flag to indicate when this property changes.
MethodTable* LoaderAllocator::FindUniqueConcreteTypeWhichImplementsThisInterface(MethodTable *pInterfaceType, CheckableConditionForOptimizationChange* pCheckableCondition)
{
    STANDARD_VM_CONTRACT;

    _ASSERTE(pInterfaceType->GetLoaderAllocator() == this);

    if (pCheckableCondition != nullptr)
        *pCheckableCondition = pInterfaceType->GenerateCheckableConditionForNewInterfaceImplementation();

    if (!pInterfaceType->HasDerivedType())
        return nullptr;

    if (pInterfaceType->HasMultipleDerivedTypes())
        return nullptr;

    // Walk derived types, and stop walking and report null if multiple concrete derived types are found.
    MethodTable *pUniqueType = nullptr;
    if (!WalkDerivingAndImplementingMethodTables(pInterfaceType, [&pUniqueType](MethodTable *pDerivedMethodTable)
    {
        if (!pDerivedMethodTable->IsAbstract())
        {
            if (pUniqueType != nullptr)
            {
                pUniqueType = nullptr;
                return false;
            }
            pUniqueType = pDerivedMethodTable;
        }

        return true;
    }))
    {
        return nullptr;
    }
    return pUniqueType;
}


#endif // !DACCESS_COMPILE
