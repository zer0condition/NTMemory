#include "core.h"

static bool g_Initialized = false;
static HANDLE g_RefreshThread = NULL;
static volatile bool g_ThreadRunning = false;
static uint32_t g_ProcessorCount = 0;
static NTSTATUS g_LastSuperfetchError = 0;

NTSTATUS Core_GetLastSuperfetchError(void) {
    return g_LastSuperfetchError;
}

static bool EnablePrivilege(LPCWSTR privilegeName) {
    HANDLE hToken;
    TOKEN_PRIVILEGES tp;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), 
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                          &hToken)) {
        return false;
    }

    if (!LookupPrivilegeValueW(NULL, privilegeName, &luid)) {
        CloseHandle(hToken);
        return false;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
    DWORD error = GetLastError();
    CloseHandle(hToken);

    return result && (error != ERROR_NOT_ALL_ASSIGNED);
}

bool Core_EnablePrivileges(void) {
    bool success = true;
    success &= EnablePrivilege(SE_DEBUG_NAME);
    success &= EnablePrivilege(SE_PROF_SINGLE_PROCESS_NAME);
    success &= EnablePrivilege(SE_SYSTEM_PROFILE_NAME);
    EnablePrivilege(SE_INCREASE_QUOTA_NAME);
    EnablePrivilege(SE_INC_BASE_PRIORITY_NAME);
    return success;
}

bool Core_IsElevated(void) {
    BOOL elevated = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &size)) {
            elevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return elevated != FALSE;
}

bool Core_Initialize(void) {
    if (g_Initialized) return true;
    
    if (!Core_IsElevated()) {
        return false;
    }
    
    Core_EnablePrivileges();
    
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    g_ProcessorCount = sysInfo.dwNumberOfProcessors;
    
    g_Initialized = true;
    return true;
}

void Core_Shutdown(void) {
    g_ThreadRunning = false;
    if (g_RefreshThread) {
        WaitForSingleObject(g_RefreshThread, 1000);
        CloseHandle(g_RefreshThread);
        g_RefreshThread = NULL;
    }
    g_Initialized = false;
}

size_t Core_GetSuperfetchInfoSize(void) {
    return sizeof(SUPERFETCH_INFORMATION);
}

bool Core_QuerySuperfetchInfo(SUPERFETCH_INFORMATION_CLASS infoClass, PVOID buffer, ULONG length, PULONG returnLength) {
    SUPERFETCH_INFORMATION sfInfo = {0};
    SUPERFETCH_INFO_INIT(&sfInfo, infoClass, buffer, length);
    
    NTSTATUS status = NtQuerySystemInformation(
        SYS_SuperfetchInformation,
        &sfInfo,
        sizeof(sfInfo),
        returnLength
    );
    
    g_LastSuperfetchError = status;
    return status == 0;
}

void Core_RefreshMemoryStats(MEMORY_STATS* stats) {
    if (!stats) return;
    
    SYS_MEMORY_LIST_INFORMATION memInfo = {0};
    NTSTATUS status = NtQuerySystemInformation(
        SYS_MemoryListInformation,
        &memInfo,
        sizeof(memInfo),
        NULL
    );
    
    if (status == 0) {
        stats->ZeroPageCount = memInfo.ZeroPageCount;
        stats->FreePageCount = memInfo.FreePageCount;
        stats->ModifiedPageCount = memInfo.ModifiedPageCount;
        stats->ModifiedNoWritePageCount = memInfo.ModifiedNoWritePageCount;
        stats->BadPageCount = memInfo.BadPageCount;
        
        stats->TotalStandby = 0;
        for (int i = 0; i < 8; i++) {
            stats->StandbyPageCount[i] = memInfo.PageCountByPriority[i];
            stats->RepurposedCount[i] = memInfo.RepurposedPagesByPriority[i];
            stats->TotalStandby += memInfo.PageCountByPriority[i];
        }
        
        stats->TotalFree = stats->ZeroPageCount + stats->FreePageCount;
    }
    
    MEMORYSTATUSEX memStatus = { .dwLength = sizeof(memStatus) };
    if (GlobalMemoryStatusEx(&memStatus)) {
        stats->TotalPhysical = memStatus.ullTotalPhys;
        stats->AvailablePhysical = memStatus.ullAvailPhys;
    }
    
    UCHAR pfBuffer[4096];
    status = NtQuerySystemInformation(
        SYS_PageFileInformation,
        pfBuffer,
        sizeof(pfBuffer),
        NULL
    );
    
    if (status == 0) {
        PSYS_PAGEFILE_INFO pfInfo = (PSYS_PAGEFILE_INFO)pfBuffer;
        stats->PagefileTotal = (uint64_t)pfInfo->TotalSize * PAGE_SIZE;
        stats->PagefileInUse = (uint64_t)pfInfo->TotalInUse * PAGE_SIZE;
        stats->PagefilePeak = (uint64_t)pfInfo->PeakUsage * PAGE_SIZE;
    }
    
    SYS_PERFORMANCE_INFO perfInfo = {0};
    status = NtQuerySystemInformation(
        SYS_PerformanceInformation,
        &perfInfo,
        sizeof(perfInfo),
        NULL
    );
    
    if (status == 0) {
        stats->PagedPoolPages = perfInfo.PagedPoolPages;
        stats->NonPagedPoolPages = perfInfo.NonPagedPoolPages;
        stats->PagedPoolAllocs = perfInfo.PagedPoolAllocs;
        stats->NonPagedPoolAllocs = perfInfo.NonPagedPoolAllocs;
        stats->PagedPoolFrees = perfInfo.PagedPoolFrees;
        stats->NonPagedPoolFrees = perfInfo.NonPagedPoolFrees;
        
        stats->CommittedPages = perfInfo.CommittedPages;
        stats->CommitLimit = perfInfo.CommitLimit;
        stats->PeakCommitment = perfInfo.PeakCommitment;
        stats->SharedCommit = perfInfo.SharedCommittedPages;
        
        stats->ContextSwitches = perfInfo.ContextSwitches;
        stats->SystemCalls = perfInfo.SystemCalls;
        stats->PageFaults = perfInfo.PageFaultCount;
        stats->CopyOnWriteCount = perfInfo.CopyOnWriteCount;
        stats->TransitionFaults = perfInfo.TransitionCount;
        stats->DemandZeroFaults = perfInfo.DemandZeroCount;
    }
    
    SYS_FILECACHE_INFO cacheInfo = {0};
    status = NtQuerySystemInformation(
        SYS_FileCacheInformation,
        &cacheInfo,
        sizeof(cacheInfo),
        NULL
    );
    
    if (status == 0) {
        stats->FileCacheSize = cacheInfo.CurrentSize;
        stats->FileCachePeak = cacheInfo.PeakSize;
        stats->FileCachePageFaults = cacheInfo.PageFaultCount;
    }
    
    stats->LastUpdate = Core_GetTickCount64();
    stats->Valid = true;
}

static int CompareProcessByWorkingSet(const void* a, const void* b) {
    const PROCESS_MEMORY_INFO* pa = (const PROCESS_MEMORY_INFO*)a;
    const PROCESS_MEMORY_INFO* pb = (const PROCESS_MEMORY_INFO*)b;
    if (pb->WorkingSet > pa->WorkingSet) return 1;
    if (pb->WorkingSet < pa->WorkingSet) return -1;
    return 0;
}

void Core_RefreshProcessList(PROCESS_LIST* list) {
    if (!list) return;
    
    DWORD pids[2048];
    DWORD cbNeeded;
    if (!EnumProcesses(pids, sizeof(pids), &cbNeeded)) {
        return;
    }
    
    DWORD processCount = cbNeeded / sizeof(DWORD);
    
    if (list->Capacity < processCount) {
        list->Capacity = processCount + 256;
        list->Processes = (PROCESS_MEMORY_INFO*)realloc(
            list->Processes, 
            list->Capacity * sizeof(PROCESS_MEMORY_INFO)
        );
    }
    
    list->Count = 0;
    list->TotalWorkingSet = 0;
    list->TotalPrivate = 0;
    
    for (DWORD i = 0; i < processCount; i++) {
        if (pids[i] == 0) continue;
        
        HANDLE hProcess = OpenProcess(
            PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
            FALSE, pids[i]
        );
        
        if (!hProcess) continue;
        
        PROCESS_MEMORY_INFO* info = &list->Processes[list->Count];
        memset(info, 0, sizeof(*info));
        info->Pid = pids[i];
        
        WCHAR exePath[MAX_PATH];
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, exePath, &pathSize)) {
            WCHAR* lastSlash = wcsrchr(exePath, L'\\');
            if (lastSlash) {
                wcsncpy_s(info->Name, 64, lastSlash + 1, 63);
            } else {
                wcsncpy_s(info->Name, 64, exePath, 63);
            }
        } else {
            wcscpy_s(info->Name, 64, L"<unknown>");
        }
        
        PROCESS_MEMORY_COUNTERS_EX pmc = { .cb = sizeof(pmc) };
        if (GetProcessMemoryInfo(hProcess, (PPROCESS_MEMORY_COUNTERS)&pmc, sizeof(pmc))) {
            info->WorkingSet = pmc.WorkingSetSize;
            info->PeakWorkingSet = pmc.PeakWorkingSetSize;
            info->PrivateBytes = pmc.PrivateUsage;
            info->PagefileUsage = pmc.PagefileUsage;
            info->PageFaults = pmc.PageFaultCount;
            
            list->TotalWorkingSet += pmc.WorkingSetSize;
            list->TotalPrivate += pmc.PrivateUsage;
        }
        
        CloseHandle(hProcess);
        list->Count++;
    }
    
    qsort(list->Processes, list->Count, sizeof(PROCESS_MEMORY_INFO), CompareProcessByWorkingSet);
    
    list->LastUpdate = Core_GetTickCount64();
}

bool Core_RefreshSuperfetchProcesses(PF_PRIVSOURCE_INFO** procs, uint32_t* count) {
    if (!procs || !count) return false;
    
    ULONG bufferSize = sizeof(PF_PRIVSOURCE_QUERY) + sizeof(PF_PRIVSOURCE_INFO) * 512;
    PPF_PRIVSOURCE_QUERY query = (PPF_PRIVSOURCE_QUERY)malloc(bufferSize);
    if (!query) return false;
    
    memset(query, 0, bufferSize);
    query->Version = 8;
    query->Flags = 0;
    
    ULONG returnLength = 0;
    if (!Core_QuerySuperfetchInfo(SuperfetchPrivSourceQuery, query, bufferSize, &returnLength)) {
        free(query);
        bufferSize = returnLength > 0 ? returnLength : bufferSize * 4;
        query = (PPF_PRIVSOURCE_QUERY)malloc(bufferSize);
        if (!query) return false;
        
        memset(query, 0, bufferSize);
        query->Version = 8;
        
        if (!Core_QuerySuperfetchInfo(SuperfetchPrivSourceQuery, query, bufferSize, &returnLength)) {
            free(query);
            return false;
        }
    }
    
    if (*procs) free(*procs);
    *count = query->InfoCount;
    
    if (*count > 0) {
        *procs = (PF_PRIVSOURCE_INFO*)malloc(sizeof(PF_PRIVSOURCE_INFO) * (*count));
        if (*procs) {
            memcpy(*procs, query->Sources, sizeof(PF_PRIVSOURCE_INFO) * (*count));
        }
    } else {
        *procs = NULL;
    }
    
    free(query);
    return true;
}

void Core_RefreshPhysicalMap(PHYSICAL_MEMORY_MAP* map) {
    if (!map) return;
    
    if (Core_QueryMemoryRangesV2(map)) {
        map->LastUpdate = Core_GetTickCount64();
        return;
    }
    
    PF_MEMORY_RANGE_INFO rangeInfo = { .Version = 1 };
    SUPERFETCH_INFORMATION sfInfo = {0};
    
    SUPERFETCH_INFO_INIT(&sfInfo, SuperfetchMemoryRangesQuery, &rangeInfo, sizeof(rangeInfo));
    
    NTSTATUS status = NtQuerySystemInformation(
        SYS_SuperfetchInformation,
        &sfInfo,
        sizeof(sfInfo),
        NULL
    );
    
    g_LastSuperfetchError = status;
    
    if (status != 0) {
        MEMORYSTATUSEX memStatus = { .dwLength = sizeof(memStatus) };
        if (GlobalMemoryStatusEx(&memStatus)) {
            if (!map->Ranges) {
                map->Ranges = (MEMORY_RANGE*)malloc(sizeof(MEMORY_RANGE));
                map->RangeCount = 1;
            }
            map->Ranges[0].StartPage = 0;
            map->Ranges[0].PageCount = memStatus.ullTotalPhys / PAGE_SIZE;
            map->Ranges[0].SizeMB = memStatus.ullTotalPhys / (1024 * 1024);
            map->TotalPages = map->Ranges[0].PageCount;
            map->TotalSizeGB = memStatus.ullTotalPhys / (1024 * 1024 * 1024);
        }
        map->LastUpdate = Core_GetTickCount64();
        return;
    }
    
    if (!map->Ranges || map->RangeCount < rangeInfo.RangeCount) {
        map->Ranges = (MEMORY_RANGE*)realloc(map->Ranges, 
            rangeInfo.RangeCount * sizeof(MEMORY_RANGE));
    }
    
    map->RangeCount = rangeInfo.RangeCount;
    map->TotalPages = 0;
    
    for (uint32_t i = 0; i < rangeInfo.RangeCount; i++) {
        map->Ranges[i].StartPage = rangeInfo.Ranges[i].StartPage;
        map->Ranges[i].PageCount = rangeInfo.Ranges[i].PageCount;
        map->Ranges[i].SizeMB = PagesToMB(rangeInfo.Ranges[i].PageCount);
        map->TotalPages += rangeInfo.Ranges[i].PageCount;
    }
    
    map->TotalSizeGB = PagesToGB(map->TotalPages);
    map->LastUpdate = Core_GetTickCount64();
}

bool Core_QueryMemoryRangesV2(PHYSICAL_MEMORY_MAP* map) {
    if (!map) return false;
    
    PF_MEMORY_RANGE_INFO_V2 infoHeader = { .Version = 2 };
    ULONG returnLength = 0;
    
    SUPERFETCH_INFORMATION sfInfo = {0};
    SUPERFETCH_INFO_INIT(&sfInfo, SuperfetchMemoryRangesQuery, &infoHeader, sizeof(infoHeader));
    
    NTSTATUS status = NtQuerySystemInformation(
        SYS_SuperfetchInformation,
        &sfInfo,
        sizeof(sfInfo),
        &returnLength
    );
    
    g_LastSuperfetchError = status;
    
    if (status != 0xC0000023 && status != 0) {
        return false;
    }
    
    ULONG bufferSize = returnLength > 0 ? returnLength : 
        sizeof(PF_MEMORY_RANGE_INFO_V2) + sizeof(PF_PHYSICAL_MEMORY_RANGE) * 256;
    
    PPF_MEMORY_RANGE_INFO_V2 info = (PPF_MEMORY_RANGE_INFO_V2)malloc(bufferSize);
    if (!info) return false;
    
    memset(info, 0, bufferSize);
    info->Version = 2;
    
    SUPERFETCH_INFO_INIT(&sfInfo, SuperfetchMemoryRangesQuery, info, bufferSize);
    
    status = NtQuerySystemInformation(
        SYS_SuperfetchInformation,
        &sfInfo,
        sizeof(sfInfo),
        NULL
    );
    
    g_LastSuperfetchError = status;
    
    if (status != 0) {
        free(info);
        return false;
    }
    
    if (!map->Ranges || map->RangeCount < info->RangeCount) {
        map->Ranges = (MEMORY_RANGE*)realloc(map->Ranges, 
            info->RangeCount * sizeof(MEMORY_RANGE));
    }
    
    map->RangeCount = info->RangeCount;
    map->TotalPages = 0;
    
    for (uint32_t i = 0; i < info->RangeCount; i++) {
        map->Ranges[i].StartPage = info->Ranges[i].BasePfn;
        map->Ranges[i].PageCount = info->Ranges[i].PageCount;
        map->Ranges[i].SizeMB = PagesToMB(info->Ranges[i].PageCount);
        map->TotalPages += info->Ranges[i].PageCount;
    }
    
    map->TotalSizeGB = PagesToGB(map->TotalPages);
    
    free(info);
    return true;
}

bool Core_QueryPfnDatabase(uint64_t startPfn, uint64_t pfnCount, PFN_QUERY_RESULTS* results) {
    if (!results || pfnCount == 0) return false;
    
    uint64_t startTime = Core_GetTickCount64();
    
    size_t bufferSize = sizeof(PF_PFN_PRIO_REQUEST) + sizeof(MMPFN_IDENTITY) * pfnCount;
    PPF_PFN_PRIO_REQUEST request = (PPF_PFN_PRIO_REQUEST)malloc(bufferSize);
    if (!request) return false;
    
    memset(request, 0, bufferSize);
    request->Version = 1;
    request->RequestFlags = 1;
    request->PfnCount = (SIZE_T)pfnCount;
    
    for (uint64_t i = 0; i < pfnCount; i++) {
        request->PageData[i].PageFrameIndex = (SIZE_T)(startPfn + i);
    }
    
    if (!Core_QuerySuperfetchInfo(SuperfetchPfnQuery, request, (ULONG)bufferSize, NULL)) {
        free(request);
        return false;
    }
    
    if (results->Capacity < (uint32_t)pfnCount) {
        results->Capacity = (uint32_t)pfnCount;
        results->Pages = (PFN_INFO*)realloc(results->Pages, 
            results->Capacity * sizeof(PFN_INFO));
    }
    
    results->Count = 0;
    
    for (uint64_t i = 0; i < pfnCount; i++) {
        PMMPFN_IDENTITY pageData = &request->PageData[i];
        PFN_INFO* info = &results->Pages[results->Count];
        
        info->PageFrameIndex = pageData->PageFrameIndex;
        info->PhysicalAddress = pageData->PageFrameIndex << PAGE_SHIFT;
        info->VirtualAddress = (uint64_t)pageData->u2.VirtualAddress;
        info->UseDescription = (uint32_t)pageData->u1.e1.UseDescription;
        info->ListDescription = (uint32_t)pageData->u1.e1.ListDescription;
        info->Priority = (uint32_t)pageData->u1.e1.Priority;
        info->Pinned = pageData->u1.e1.Pinned != 0;
        info->Image = pageData->u2.e1.Image != 0;
        info->Valid = (info->VirtualAddress != 0);
        
        results->Count++;
    }
    
    results->QueryTimeMs = Core_GetTickCount64() - startTime;
    results->LastUpdate = Core_GetTickCount64();
    
    free(request);
    return true;
}

bool Core_TranslateVirtualAddress(PVOID virtualAddr, uint64_t* physicalAddr, PFN_INFO* info) {
    if (!virtualAddr || !physicalAddr) return false;
    
    // Query V2 memory ranges first for complete superfetch information
    PPF_MEMORY_RANGE_INFO_V2 rangeInfoV2 = NULL;
    ULONG bufferSize = sizeof(PF_MEMORY_RANGE_INFO_V2) + sizeof(PF_PHYSICAL_MEMORY_RANGE) * 512;
    rangeInfoV2 = (PPF_MEMORY_RANGE_INFO_V2)malloc(bufferSize);
    if (!rangeInfoV2) return false;
    
    memset(rangeInfoV2, 0, bufferSize);
    rangeInfoV2->Version = 2;
    
    ULONG returnLength = 0;
    if (!Core_QuerySuperfetchInfo(SuperfetchMemoryRangesQuery, rangeInfoV2, bufferSize, &returnLength)) {
        // Retry with larger buffer if needed
        if (returnLength > bufferSize) {
            free(rangeInfoV2);
            bufferSize = returnLength;
            rangeInfoV2 = (PPF_MEMORY_RANGE_INFO_V2)malloc(bufferSize);
            if (!rangeInfoV2) return false;
            
            memset(rangeInfoV2, 0, bufferSize);
            rangeInfoV2->Version = 2;
            
            if (!Core_QuerySuperfetchInfo(SuperfetchMemoryRangesQuery, rangeInfoV2, bufferSize, NULL)) {
                // Fallback to V1 ranges
                free(rangeInfoV2);
                rangeInfoV2 = NULL;
            }
        } else {
            free(rangeInfoV2);
            rangeInfoV2 = NULL;
        }
    }
    
    // Build complete PFN list from all superfetch ranges
    uint64_t totalPfnCount = 0;
    uint32_t rangeCount = 0;
    
    if (rangeInfoV2) {
        rangeCount = rangeInfoV2->RangeCount;
        for (uint32_t i = 0; i < rangeCount; i++) {
            totalPfnCount += rangeInfoV2->Ranges[i].PageCount;
        }
    } else {
        // Use V1 fallback
        PF_MEMORY_RANGE_INFO rangeInfoV1 = { .Version = 1 };
        if (!Core_QuerySuperfetchInfo(SuperfetchMemoryRangesQuery, &rangeInfoV1, sizeof(rangeInfoV1), NULL)) {
            return false;
        }
        rangeCount = rangeInfoV1.RangeCount;
        for (uint32_t i = 0; i < rangeCount; i++) {
            totalPfnCount += rangeInfoV1.Ranges[i].PageCount;
        }
    }
    
    // Iterate through all superfetch ranges to find the virtual address
    uint32_t currentRange = 0;
    uint64_t rangeOffset = 0;
    
    while (currentRange < rangeCount) {
        uint64_t basePfn, pageCount;
        
        if (rangeInfoV2) {
            basePfn = rangeInfoV2->Ranges[currentRange].BasePfn;
            pageCount = rangeInfoV2->Ranges[currentRange].PageCount;
        } else {
            PF_MEMORY_RANGE_INFO rangeInfoV1 = { .Version = 1 };
            Core_QuerySuperfetchInfo(SuperfetchMemoryRangesQuery, &rangeInfoV1, sizeof(rangeInfoV1), NULL);
            basePfn = rangeInfoV1.Ranges[currentRange].StartPage;
            pageCount = rangeInfoV1.Ranges[currentRange].PageCount;
        }
        
        // Process entire range through superfetch
        const uint64_t chunkSize = 8192;  // Larger chunks for efficiency
        
        for (uint64_t offset = rangeOffset; offset < pageCount; offset += chunkSize) {
            uint64_t queryCount = (pageCount - offset < chunkSize) ? (pageCount - offset) : chunkSize;
            
            size_t reqBufferSize = sizeof(PF_PFN_PRIO_REQUEST) + sizeof(MMPFN_IDENTITY) * queryCount;
            PPF_PFN_PRIO_REQUEST request = (PPF_PFN_PRIO_REQUEST)malloc(reqBufferSize);
            if (!request) continue;
            
            memset(request, 0, reqBufferSize);
            request->Version = 1;
            request->RequestFlags = 1;
            request->PfnCount = (SIZE_T)queryCount;
            
            // Fill in all PFNs for this chunk
            for (uint64_t i = 0; i < queryCount; i++) {
                request->PageData[i].PageFrameIndex = (SIZE_T)(basePfn + offset + i);
            }
            
            // Query superfetch for this chunk - get all available data
            if (Core_QuerySuperfetchInfo(SuperfetchPfnQuery, request, (ULONG)reqBufferSize, NULL)) {
                // Search through all returned entries
                for (uint64_t i = 0; i < queryCount; i++) {
                    PMMPFN_IDENTITY pageData = &request->PageData[i];
                    PVOID mappedVa = pageData->u2.VirtualAddress;
                    
                    // Check if this entry's VA matches our target (page-aligned comparison)
                    if (mappedVa && ((ULONG_PTR)mappedVa & ~0xFFF) == ((ULONG_PTR)virtualAddr & ~0xFFF)) {
                        *physicalAddr = (pageData->PageFrameIndex << PAGE_SHIFT) | 
                                       ((ULONG_PTR)virtualAddr & 0xFFF);
                        
                        if (info) {
                            info->PageFrameIndex = pageData->PageFrameIndex;
                            info->PhysicalAddress = *physicalAddr;
                            info->VirtualAddress = (uint64_t)virtualAddr;
                            info->UseDescription = (uint32_t)pageData->u1.e1.UseDescription;
                            info->ListDescription = (uint32_t)pageData->u1.e1.ListDescription;
                            info->Priority = (uint32_t)pageData->u1.e1.Priority;
                            info->Pinned = pageData->u1.e1.Pinned != 0;
                            info->Image = pageData->u2.e1.Image != 0;
                            info->Valid = true;
                        }
                        
                        free(request);
                        if (rangeInfoV2) free(rangeInfoV2);
                        return true;
                    }
                }
            }
            
            free(request);
        }
        
        rangeOffset = 0;  // Reset offset for next range
        currentRange++;
    }
    
    if (rangeInfoV2) free(rangeInfoV2);
    return false;
}

void Core_RefreshPerformanceStats(PERF_STATS* stats) {
    if (!stats) return;
    
    if (stats->ProcessorCount != g_ProcessorCount) {
        stats->ProcessorCount = g_ProcessorCount;
        stats->IdleTime = (uint64_t*)realloc(stats->IdleTime, g_ProcessorCount * sizeof(uint64_t));
        stats->KernelTime = (uint64_t*)realloc(stats->KernelTime, g_ProcessorCount * sizeof(uint64_t));
        stats->UserTime = (uint64_t*)realloc(stats->UserTime, g_ProcessorCount * sizeof(uint64_t));
        stats->DpcTime = (uint64_t*)realloc(stats->DpcTime, g_ProcessorCount * sizeof(uint64_t));
        stats->InterruptTime = (uint64_t*)realloc(stats->InterruptTime, g_ProcessorCount * sizeof(uint64_t));
        stats->InterruptCount = (uint32_t*)realloc(stats->InterruptCount, g_ProcessorCount * sizeof(uint32_t));
        stats->DpcCount = (uint32_t*)realloc(stats->DpcCount, g_ProcessorCount * sizeof(uint32_t));
    }
    
    size_t bufferSize = sizeof(SYS_PROCESSOR_PERF_INFO) * g_ProcessorCount;
    PSYS_PROCESSOR_PERF_INFO procPerf = 
        (PSYS_PROCESSOR_PERF_INFO)malloc(bufferSize);
    
    if (procPerf) {
        NTSTATUS status = NtQuerySystemInformation(
            SYS_ProcessorPerformanceInformation,
            procPerf,
            (ULONG)bufferSize,
            NULL
        );
        
        if (status == 0) {
            stats->TotalIdleTime = 0;
            stats->TotalKernelTime = 0;
            stats->TotalUserTime = 0;
            
            for (uint32_t i = 0; i < g_ProcessorCount; i++) {
                stats->IdleTime[i] = procPerf[i].IdleTime.QuadPart;
                stats->KernelTime[i] = procPerf[i].KernelTime.QuadPart;
                stats->UserTime[i] = procPerf[i].UserTime.QuadPart;
                stats->DpcTime[i] = procPerf[i].DpcTime.QuadPart;
                stats->InterruptTime[i] = procPerf[i].InterruptTime.QuadPart;
                stats->InterruptCount[i] = procPerf[i].InterruptCount;
                
                stats->TotalIdleTime += procPerf[i].IdleTime.QuadPart;
                stats->TotalKernelTime += procPerf[i].KernelTime.QuadPart;
                stats->TotalUserTime += procPerf[i].UserTime.QuadPart;
            }
        }
        
        free(procPerf);
    }
    
    size_t intBufferSize = sizeof(SYS_INTERRUPT_INFO) * g_ProcessorCount;
    PSYS_INTERRUPT_INFO intInfo = 
        (PSYS_INTERRUPT_INFO)malloc(intBufferSize);
    
    if (intInfo) {
        NTSTATUS status = NtQuerySystemInformation(
            SYS_InterruptInformation,
            intInfo,
            (ULONG)intBufferSize,
            NULL
        );
        
        if (status == 0) {
            for (uint32_t i = 0; i < g_ProcessorCount; i++) {
                stats->DpcCount[i] = intInfo[i].DpcCount;
            }
        }
        
        free(intInfo);
    }
    
    SYS_PERFORMANCE_INFO perfInfo = {0};
    NTSTATUS status = NtQuerySystemInformation(
        SYS_PerformanceInformation,
        &perfInfo,
        sizeof(perfInfo),
        NULL
    );
    
    if (status == 0) {
        stats->IoReadBytes = perfInfo.IoReadTransferCount.QuadPart;
        stats->IoWriteBytes = perfInfo.IoWriteTransferCount.QuadPart;
        stats->IoOtherBytes = perfInfo.IoOtherTransferCount.QuadPart;
        stats->IoReadOps = perfInfo.IoReadOperationCount;
        stats->IoWriteOps = perfInfo.IoWriteOperationCount;
        stats->IoOtherOps = perfInfo.IoOtherOperationCount;
    }
    
    stats->LastUpdate = Core_GetTickCount64();
    stats->Valid = true;
}

static int ComparePoolTagByUsage(const void* a, const void* b) {
    const POOLTAG_INFO* pa = (const POOLTAG_INFO*)a;
    const POOLTAG_INFO* pb = (const POOLTAG_INFO*)b;
    if (pb->TotalUsed > pa->TotalUsed) return 1;
    if (pb->TotalUsed < pa->TotalUsed) return -1;
    return 0;
}

void Core_RefreshPoolTags(POOLTAG_LIST* list) {
    if (!list) return;
    
    ULONG bufferSize = 1024 * 1024;
    PSYS_POOLTAG_INFO poolInfo = (PSYS_POOLTAG_INFO)malloc(bufferSize);
    if (!poolInfo) return;
    
    NTSTATUS status = NtQuerySystemInformation(
        SYS_PoolTagInformation,
        poolInfo,
        bufferSize,
        NULL
    );
    
    if (status != 0) {
        free(poolInfo);
        return;
    }
    
    if (list->Capacity < poolInfo->Count) {
        list->Capacity = poolInfo->Count + 256;
        list->Tags = (POOLTAG_INFO*)realloc(list->Tags, 
            list->Capacity * sizeof(POOLTAG_INFO));
    }
    
    list->Count = poolInfo->Count;
    list->TotalPagedUsed = 0;
    list->TotalNonPagedUsed = 0;
    
    for (ULONG i = 0; i < poolInfo->Count; i++) {
        PSYS_POOLTAG tag = &poolInfo->TagInfo[i];
        POOLTAG_INFO* info = &list->Tags[i];
        
        info->Tag[0] = tag->Tag[0];
        info->Tag[1] = tag->Tag[1];
        info->Tag[2] = tag->Tag[2];
        info->Tag[3] = tag->Tag[3];
        info->Tag[4] = 0;
        
        info->PagedAllocs = tag->PagedAllocs;
        info->PagedFrees = tag->PagedFrees;
        info->PagedUsed = tag->PagedUsed;
        info->NonPagedAllocs = tag->NonPagedAllocs;
        info->NonPagedFrees = tag->NonPagedFrees;
        info->NonPagedUsed = tag->NonPagedUsed;
        info->TotalUsed = tag->PagedUsed + tag->NonPagedUsed;
        
        list->TotalPagedUsed += tag->PagedUsed;
        list->TotalNonPagedUsed += tag->NonPagedUsed;
    }
    
    qsort(list->Tags, list->Count, sizeof(POOLTAG_INFO), ComparePoolTagByUsage);
    
    free(poolInfo);
    list->LastUpdate = Core_GetTickCount64();
}

// Known Windows object types (common indices may vary slightly between versions)
static const char* g_KnownObjectTypes[] = {
    "",             // 0
    "",             // 1
    "Type",         // 2
    "Directory",    // 3
    "SymbolicLink", // 4
    "Token",        // 5
    "Job",          // 6
    "Process",      // 7
    "Thread",       // 8
    "Partition",    // 9
    "UserApcReserve",// 10
    "IoCompletionReserve", // 11
    "ActivityReference",  // 12
    "PsSiloContextPaged", // 13
    "PsSiloContextNonPaged", // 14
    "DebugObject",  // 15
    "Event",        // 16
    "Mutant",       // 17
    "Callback",     // 18
    "Semaphore",    // 19
    "Timer",        // 20
    "IRTimer",      // 21
    "Profile",      // 22
    "KeyedEvent",   // 23
    "WindowStation",// 24
    "Desktop",      // 25
    "Composition",  // 26
    "RawInputManager", // 27
    "CoreMessaging",// 28
    "ActivationObject", // 29
    "TpWorkerFactory", // 30
    "Adapter",      // 31
    "Controller",   // 32
    "Device",       // 33
    "Driver",       // 34
    "IoCompletion", // 35
    "WaitCompletionPacket", // 36
    "File",         // 37
    "TmTm",         // 38
    "TmTx",         // 39
    "TmRm",         // 40
    "TmEn",         // 41
    "Section",      // 42
    "Session",      // 43
    "Key",          // 44
    "RegistryTransaction", // 45
    "ALPC Port",    // 46
    "EnergyTracker",// 47
    "PowerRequest", // 48
    "WmiGuid",      // 49
    "EtwRegistration", // 50
    "EtwSessionDemuxEntry", // 51
    "EtwConsumer",  // 52
    "CoverageSampler", // 53
    "DmaAdapter",   // 54
    "PcwObject",    // 55
    "FilterConnectionPort", // 56
    "FilterCommunicationPort", // 57
    "NdisCmState",  // 58
    "DxgkSharedResource", // 59
    "DxgkSharedSyncObject", // 60
    "DxgkSharedSwapChainObject", // 61
    "DxgkDisplayManagerObject", // 62
    "DxgkCurrentDxgProcessObject" // 63
};

static int CompareHandleProcessInfo(const void* a, const void* b) {
    const HANDLE_PROCESS_INFO* pa = (const HANDLE_PROCESS_INFO*)a;
    const HANDLE_PROCESS_INFO* pb = (const HANDLE_PROCESS_INFO*)b;
    if (pb->HandleCount > pa->HandleCount) return 1;
    if (pb->HandleCount < pa->HandleCount) return -1;
    return 0;
}

void Core_RefreshHandleStats(HANDLE_STATS* stats) {
    if (!stats) return;
    
    ULONG bufferSize = 16 * 1024 * 1024;
    PSYS_HANDLE_INFO_EX handleInfo = NULL;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        handleInfo = (PSYS_HANDLE_INFO_EX)malloc(bufferSize);
        if (!handleInfo) return;
        
        NTSTATUS status = NtQuerySystemInformation(
            SYS_ExtendedHandleInformation,
            handleInfo,
            bufferSize,
            NULL
        );
        
        if (status == 0) break;
        
        free(handleInfo);
        handleInfo = NULL;
        bufferSize *= 2;
    }
    
    if (!handleInfo) return;
    
    stats->TotalHandles = handleInfo->NumberOfHandles;
    
    memset(stats->HandlesByType, 0, sizeof(stats->HandlesByType));
    memset(stats->TypeNames, 0, sizeof(stats->TypeNames));
    // Note: stats->Processes is freed and reallocated later
    
    // Copy known type names
    for (int i = 0; i < 64 && i < (int)(sizeof(g_KnownObjectTypes)/sizeof(g_KnownObjectTypes[0])); i++) {
        strncpy(stats->TypeNames[i], g_KnownObjectTypes[i], 31);
    }
    
    // Track per-process handle counts with type breakdown
    #define MAX_TRACK_PROCS 1024
    typedef struct { 
        ULONG pid; 
        uint32_t count;
        uint32_t fileCount;
        uint32_t keyCount;
        uint32_t eventCount;
        uint32_t mutantCount;
        uint32_t sectionCount;
        uint32_t threadCount;
        uint32_t processCount;
        uint32_t tokenCount;
        uint32_t otherCount;
    } ProcCount;
    ProcCount* procCounts = (ProcCount*)calloc(MAX_TRACK_PROCS, sizeof(ProcCount));
    int procCountIdx = 0;
    
    ULONG_PTR lastPid = ULONG_MAX;
    int lastProcIdx = -1;
    uint64_t uniqueProcesses = 0;
    
    for (ULONG_PTR i = 0; i < handleInfo->NumberOfHandles; i++) {
        PSYS_HANDLE_ENTRY_EX entry = &handleInfo->Handles[i];
        USHORT typeIdx = entry->ObjectTypeIndex;
        
        // Count by type
        if (typeIdx < 64) {
            stats->HandlesByType[typeIdx]++;
        }
        
        // Count by process
        if (entry->UniqueProcessId != lastPid) {
            uniqueProcesses++;
            lastPid = entry->UniqueProcessId;
            lastProcIdx = -1;
            
            // Find existing or add new
            for (int j = 0; j < procCountIdx; j++) {
                if (procCounts[j].pid == (ULONG)entry->UniqueProcessId) {
                    lastProcIdx = j;
                    break;
                }
            }
            if (lastProcIdx < 0 && procCountIdx < MAX_TRACK_PROCS) {
                lastProcIdx = procCountIdx++;
                procCounts[lastProcIdx].pid = (ULONG)entry->UniqueProcessId;
                memset(&procCounts[lastProcIdx].count, 0, sizeof(ProcCount) - sizeof(ULONG));
            }
        }
        
        if (lastProcIdx >= 0) {
            procCounts[lastProcIdx].count++;
            // Track by type (common object type indices on Windows 10/11)
            switch (typeIdx) {
                case 37: procCounts[lastProcIdx].fileCount++; break;      // File
                case 44: procCounts[lastProcIdx].keyCount++; break;       // Key (Registry)
                case 16: procCounts[lastProcIdx].eventCount++; break;     // Event
                case 17: procCounts[lastProcIdx].mutantCount++; break;    // Mutant
                case 42: procCounts[lastProcIdx].sectionCount++; break;   // Section
                case 8:  procCounts[lastProcIdx].threadCount++; break;    // Thread
                case 7:  procCounts[lastProcIdx].processCount++; break;   // Process
                case 5:  procCounts[lastProcIdx].tokenCount++; break;     // Token
                default: procCounts[lastProcIdx].otherCount++; break;
            }
        }
    }
    
    stats->UniqueProcesses = uniqueProcesses;
    
    stats->TypeCount = 0;
    for (int i = 0; i < 64; i++) {
        if (stats->HandlesByType[i] > 0) {
            stats->TypeCount++;
        }
    }
    
    // Sort processes by handle count (bubble sort)
    for (int i = 0; i < procCountIdx - 1; i++) {
        for (int j = 0; j < procCountIdx - i - 1; j++) {
            if (procCounts[j].count < procCounts[j + 1].count) {
                ProcCount temp = procCounts[j];
                procCounts[j] = procCounts[j + 1];
                procCounts[j + 1] = temp;
            }
        }
    }
    
    // Free old process list
    if (stats->Processes) {
        free(stats->Processes);
        stats->Processes = NULL;
    }
    
    // Allocate for all processes
    stats->ProcessCount = (uint32_t)procCountIdx;
    stats->ProcessCapacity = stats->ProcessCount;
    stats->Processes = (HANDLE_PROCESS_INFO*)calloc(stats->ProcessCount, sizeof(HANDLE_PROCESS_INFO));
    
    if (!stats->Processes) {
        stats->ProcessCount = 0;
        free(procCounts);
        free(handleInfo);
        return;
    }
    
    for (uint32_t i = 0; i < stats->ProcessCount; i++) {
        stats->Processes[i].ProcessId = procCounts[i].pid;
        stats->Processes[i].HandleCount = procCounts[i].count;
        stats->Processes[i].FileHandles = procCounts[i].fileCount;
        stats->Processes[i].KeyHandles = procCounts[i].keyCount;
        stats->Processes[i].EventHandles = procCounts[i].eventCount;
        stats->Processes[i].MutantHandles = procCounts[i].mutantCount;
        stats->Processes[i].SectionHandles = procCounts[i].sectionCount;
        stats->Processes[i].ThreadHandles = procCounts[i].threadCount;
        stats->Processes[i].ProcessHandles = procCounts[i].processCount;
        stats->Processes[i].TokenHandles = procCounts[i].tokenCount;
        stats->Processes[i].OtherHandles = procCounts[i].otherCount;
        stats->Processes[i].ProcessName[0] = L'\0';
        
        // Get process name
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, procCounts[i].pid);
        if (hProc) {
            WCHAR path[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            if (QueryFullProcessImageNameW(hProc, 0, path, &size)) {
                WCHAR* name = wcsrchr(path, L'\\');
                if (name) {
                    wcsncpy(stats->Processes[i].ProcessName, name + 1, 63);
                } else {
                    wcsncpy(stats->Processes[i].ProcessName, path, 63);
                }
            }
            CloseHandle(hProc);
        }
        
        // Special names for system processes
        if (stats->Processes[i].ProcessName[0] == L'\0') {
            if (procCounts[i].pid == 0) {
                wcscpy(stats->Processes[i].ProcessName, L"System Idle");
            } else if (procCounts[i].pid == 4) {
                wcscpy(stats->Processes[i].ProcessName, L"System");
            } else {
                swprintf(stats->Processes[i].ProcessName, 63, L"PID %u", procCounts[i].pid);
            }
        }
    }
    
    free(procCounts);
    free(handleInfo);
    stats->LastUpdate = Core_GetTickCount64();
}

// Get detailed handle list for a specific process or type
void Core_RefreshHandleDetails(HANDLE_DETAIL_LIST* list, ULONG filterPid, USHORT filterType) {
    if (!list) return;
    
    // Free existing
    if (list->Handles) {
        free(list->Handles);
        list->Handles = NULL;
    }
    list->Count = 0;
    list->FilterPid = filterPid;
    
    ULONG bufferSize = 16 * 1024 * 1024;
    PSYS_HANDLE_INFO_EX handleInfo = NULL;
    
    for (int attempt = 0; attempt < 3; attempt++) {
        handleInfo = (PSYS_HANDLE_INFO_EX)malloc(bufferSize);
        if (!handleInfo) return;
        
        NTSTATUS status = NtQuerySystemInformation(
            SYS_ExtendedHandleInformation,
            handleInfo,
            bufferSize,
            NULL
        );
        
        if (status == 0) break;
        
        free(handleInfo);
        handleInfo = NULL;
        bufferSize *= 2;
    }
    
    if (!handleInfo) return;
    
    // Count matching handles first
    uint32_t matchCount = 0;
    for (ULONG_PTR i = 0; i < handleInfo->NumberOfHandles; i++) {
        PSYS_HANDLE_ENTRY_EX entry = &handleInfo->Handles[i];
        
        bool match = true;
        if (filterPid != 0 && (ULONG)entry->UniqueProcessId != filterPid) match = false;
        if (filterType != 0xFFFF && entry->ObjectTypeIndex != filterType) match = false;
        
        if (match) matchCount++;
    }
    
    // Limit to 5000 for UI performance
    uint32_t maxHandles = (matchCount > 5000) ? 5000 : matchCount;
    
    list->Handles = (HANDLE_DETAIL*)calloc(maxHandles, sizeof(HANDLE_DETAIL));
    if (!list->Handles) {
        free(handleInfo);
        return;
    }
    list->Capacity = maxHandles;
    
    uint32_t idx = 0;
    for (ULONG_PTR i = 0; i < handleInfo->NumberOfHandles && idx < maxHandles; i++) {
        PSYS_HANDLE_ENTRY_EX entry = &handleInfo->Handles[i];
        
        bool match = true;
        if (filterPid != 0 && (ULONG)entry->UniqueProcessId != filterPid) match = false;
        if (filterType != 0xFFFF && entry->ObjectTypeIndex != filterType) match = false;
        
        if (match) {
            HANDLE_DETAIL* det = &list->Handles[idx++];
            det->ProcessId = (ULONG)entry->UniqueProcessId;
            det->HandleValue = entry->HandleValue;
            det->ObjectAddress = (ULONG_PTR)entry->Object;
            det->GrantedAccess = entry->GrantedAccess;
            det->ObjectTypeIndex = entry->ObjectTypeIndex;
            det->NameResolved = false;
            det->ObjectName[0] = L'\0';
            
            // Copy type name
            if (entry->ObjectTypeIndex < 64) {
                strncpy(det->TypeName, g_KnownObjectTypes[entry->ObjectTypeIndex], 31);
            } else {
                snprintf(det->TypeName, 31, "Type %u", entry->ObjectTypeIndex);
            }
        }
    }
    
    list->Count = idx;
    list->LastUpdate = Core_GetTickCount64();
    
    free(handleInfo);
}

void Core_FreeHandleDetails(HANDLE_DETAIL_LIST* list) {
    if (list && list->Handles) {
        free(list->Handles);
        list->Handles = NULL;
        list->Count = 0;
        list->Capacity = 0;
    }
}

void Core_RefreshCompressionStats(COMPRESSION_STATS* stats) {
    if (!stats) return;
    
    SM_MEM_COMPRESSION_INFO compInfo = { .Version = 1 };
    SYSTEM_STORE_INFORMATION storeInfo = {
        .Version = 1,
        .StoreInformationClass = 4,
        .Data = &compInfo,
        .Length = sizeof(compInfo)
    };
    
    NTSTATUS status = NtQuerySystemInformation(
        SYS_StoreInformation,
        &storeInfo,
        sizeof(storeInfo),
        NULL
    );
    
    if (status == 0) {
        stats->CompressionPid = compInfo.CompressionPid;
        stats->WorkingSetSize = compInfo.WorkingSetSize;
        stats->TotalDataCompressed = compInfo.TotalDataCompressed;
        stats->TotalCompressedSize = compInfo.TotalCompressedSize;
        
        if (compInfo.TotalCompressedSize > 0) {
            stats->CompressionSavings = compInfo.TotalDataCompressed - compInfo.TotalCompressedSize;
            stats->CompressionRatio = (float)compInfo.TotalDataCompressed / (float)compInfo.TotalCompressedSize;
        } else {
            stats->CompressionSavings = 0;
            stats->CompressionRatio = 1.0f;
        }
        
        stats->Available = true;
    } else {
        stats->Available = false;
    }
    
    stats->LastUpdate = Core_GetTickCount64();
}

static int ComparePrefetchByTime(const void* a, const void* b) {
    const PREFETCH_ENTRY* pa = (const PREFETCH_ENTRY*)a;
    const PREFETCH_ENTRY* pb = (const PREFETCH_ENTRY*)b;
    if (pb->LastAccess > pa->LastAccess) return 1;
    if (pb->LastAccess < pa->LastAccess) return -1;
    return 0;
}

void Core_RefreshPrefetchList(PREFETCH_LIST* list) {
    if (!list) return;
    
    WIN32_FIND_DATAW findData;
    WCHAR searchPath[MAX_PATH];
    GetWindowsDirectoryW(searchPath, MAX_PATH);
    wcscat_s(searchPath, MAX_PATH, L"\\Prefetch\\*.pf");
    
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    uint32_t count = 0;
    do {
        count++;
    } while (FindNextFileW(hFind, &findData) && count < 2000);
    FindClose(hFind);
    
    if (list->Capacity < count) {
        list->Capacity = count + 128;
        list->Entries = (PREFETCH_ENTRY*)realloc(
            list->Entries, 
            list->Capacity * sizeof(PREFETCH_ENTRY)
        );
    }
    
    hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    list->Count = 0;
    do {
        PREFETCH_ENTRY* entry = &list->Entries[list->Count];
        
        wcsncpy_s(entry->Name, 128, findData.cFileName, 127);
        entry->Size = findData.nFileSizeLow;
        
        ULARGE_INTEGER uli;
        uli.LowPart = findData.ftLastWriteTime.dwLowDateTime;
        uli.HighPart = findData.ftLastWriteTime.dwHighDateTime;
        entry->LastAccess = uli.QuadPart;
        
        wcsncpy_s(entry->CleanName, 64, findData.cFileName, 63);
        WCHAR* dash = wcsrchr(entry->CleanName, L'-');
        if (dash) *dash = 0;
        
        list->Count++;
    } while (FindNextFileW(hFind, &findData) && list->Count < list->Capacity);
    
    FindClose(hFind);
    
    qsort(list->Entries, list->Count, sizeof(PREFETCH_ENTRY), ComparePrefetchByTime);
    
    list->LastUpdate = Core_GetTickCount64();
}

const char* Core_FormatBytes(uint64_t bytes, char* buffer, size_t bufferSize) {
    if (bytes >= 1024ULL * 1024 * 1024 * 1024) {
        snprintf(buffer, bufferSize, "%.2f TB", (double)bytes / (1024.0 * 1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024 * 1024) {
        snprintf(buffer, bufferSize, "%.2f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    } else if (bytes >= 1024ULL * 1024) {
        snprintf(buffer, bufferSize, "%.2f MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        snprintf(buffer, bufferSize, "%.2f KB", (double)bytes / 1024.0);
    } else {
        snprintf(buffer, bufferSize, "%llu B", bytes);
    }
    return buffer;
}

const char* Core_FormatPages(uint64_t pages, char* buffer, size_t bufferSize) {
    return Core_FormatBytes(pages * PAGE_SIZE, buffer, bufferSize);
}

const char* Core_FormatNumber(uint64_t num, char* buffer, size_t bufferSize) {
    if (num >= 1000000000ULL) {
        snprintf(buffer, bufferSize, "%.2fB", (double)num / 1000000000.0);
    } else if (num >= 1000000ULL) {
        snprintf(buffer, bufferSize, "%.2fM", (double)num / 1000000.0);
    } else if (num >= 1000ULL) {
        snprintf(buffer, bufferSize, "%.2fK", (double)num / 1000.0);
    } else {
        snprintf(buffer, bufferSize, "%llu", num);
    }
    return buffer;
}

uint64_t Core_GetTickCount64(void) {
    return GetTickCount64();
}

uint64_t Core_GetKernelObject(ULONG processId, HANDLE handle) {
    if (handle == NULL || handle == INVALID_HANDLE_VALUE) return 0;
    
    PSYS_HANDLE_INFO pHandleInfo = NULL;
    ULONG bufferSize = 0;
    NTSTATUS status;
    uint64_t result = 0;
    
    while ((status = NtQuerySystemInformation(
        (SYSTEM_INFORMATION_CLASS)SYS_HandleInformation,
        pHandleInfo,
        bufferSize,
        &bufferSize)) == 0xC0000004L)
    {
        if (pHandleInfo != NULL) {
            pHandleInfo = (PSYS_HANDLE_INFO)HeapReAlloc(GetProcessHeap(), 
                HEAP_ZERO_MEMORY, pHandleInfo, (size_t)bufferSize * 2);
        } else {
            pHandleInfo = (PSYS_HANDLE_INFO)HeapAlloc(GetProcessHeap(), 
                HEAP_ZERO_MEMORY, (size_t)bufferSize * 2);
        }
        bufferSize *= 2;
    }
    
    if (status != 0 || pHandleInfo == NULL) {
        if (pHandleInfo) HeapFree(GetProcessHeap(), 0, pHandleInfo);
        return 0;
    }
    
    USHORT targetHandle = (USHORT)(ULONG_PTR)handle;
    for (ULONG i = 0; i < pHandleInfo->NumberOfHandles; i++) {
        if (pHandleInfo->Handles[i].UniqueProcessId == (USHORT)processId &&
            pHandleInfo->Handles[i].HandleValue == targetHandle) {
            result = (uint64_t)pHandleInfo->Handles[i].Object;
            break;
        }
    }
    
    HeapFree(GetProcessHeap(), 0, pHandleInfo);
    return result;
}

uint64_t Core_GetEProcess(ULONG processId) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (hProcess == NULL) return 0;
    
    uint64_t eprocess = Core_GetKernelObject(GetCurrentProcessId(), hProcess);
    CloseHandle(hProcess);
    return eprocess;
}

uint64_t Core_GetCurrentEProcess(void) {
    return Core_GetEProcess(GetCurrentProcessId());
}

uint64_t Core_GetKThread(ULONG processId, ULONG threadId) {
    HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, threadId);
    if (hThread == NULL) {
        hThread = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, threadId);
    }
    if (hThread == NULL) return 0;
    
    uint64_t kthread = Core_GetKernelObject(GetCurrentProcessId(), hThread);
    CloseHandle(hThread);
    return kthread;
}

uint64_t Core_GetCurrentKThread(void) {
    return Core_GetKThread(GetCurrentProcessId(), GetCurrentThreadId());
}

bool Core_GetKernelProcessInfo(ULONG processId, KERNEL_OBJECT_INFO* info) {
    if (!info) return false;
    
    // Preserve existing image name if set (filled by toolhelp snapshot)
    wchar_t savedName[64] = {0};
    if (info->ImageName[0] != L'\0') {
        wcsncpy(savedName, info->ImageName, 63);
    }
    ULONG savedParentPid = info->ParentPid;
    ULONG savedThreadCount = info->ThreadCount;
    
    memset(info, 0, sizeof(*info));
    info->ProcessId = processId;
    info->ParentPid = savedParentPid;
    info->ThreadCount = savedThreadCount;
    
    // Restore saved name
    if (savedName[0] != L'\0') {
        wcsncpy(info->ImageName, savedName, 63);
    }
    
    info->EProcess = Core_GetEProcess(processId);
    if (info->EProcess == 0) {
        info->Valid = (savedName[0] != L'\0');  // Still valid if we have name
        return info->Valid;
    }
    
    info->Valid = true;
    
    // Open process with as much access as possible
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) {
        hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    }
    
    if (hProcess) {
        WCHAR imagePath[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameW(hProcess, 0, imagePath, &size)) {
            WCHAR* fileName = wcsrchr(imagePath, L'\\');
            if (fileName) {
                wcsncpy(info->ImageName, fileName + 1, 63);
            } else {
                wcsncpy(info->ImageName, imagePath, 63);
            }
        }
        
        DWORD sessionId = 0;
        if (ProcessIdToSessionId(processId, &sessionId)) {
            info->SessionId = sessionId;
        }
        
        // Get PEB address via NtQueryInformationProcess
        struct {
            NTSTATUS ExitStatus;
            PVOID PebBaseAddress;
            ULONG_PTR AffinityMask;
            LONG BasePriority;
            ULONG_PTR UniqueProcessId;
            ULONG_PTR InheritedFromUniqueProcessId;
        } pbi = {0};
        ULONG returnLength = 0;
        NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, 
            &pbi, sizeof(pbi), &returnLength);
        if (NT_SUCCESS(status)) {
            info->Peb = (uint64_t)pbi.PebBaseAddress;
            info->InheritedFromPid = pbi.InheritedFromUniqueProcessId;
        }
        
        // Check if WOW64 process
        ULONG_PTR wow64Info = 0;
        status = NtQueryInformationProcess(hProcess, ProcessWow64Information,
            &wow64Info, sizeof(wow64Info), NULL);
        if (NT_SUCCESS(status)) {
            info->IsWow64 = (wow64Info != 0);
        }
        
        // Get handle count
        ULONG handleCount = 0;
        if (GetProcessHandleCount(hProcess, &handleCount)) {
            info->HandleCount = handleCount;
        }
        
        // Get times
        FILETIME createTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
            info->CreateTime = ((ULONG64)createTime.dwHighDateTime << 32) | createTime.dwLowDateTime;
            info->KernelTime = ((ULONG64)kernelTime.dwHighDateTime << 32) | kernelTime.dwLowDateTime;
            info->UserTime = ((ULONG64)userTime.dwHighDateTime << 32) | userTime.dwLowDateTime;
        }
        
        CloseHandle(hProcess);
    }
    
    // Get extended process info from system information
    PVOID procBuffer = Core_GetSystemInformation(SYS_ProcessInformation);
    if (procBuffer) {
        SYSTEM_PROCESS_INFORMATION* proc = (SYSTEM_PROCESS_INFORMATION*)procBuffer;
        while (proc) {
            if ((ULONG)(ULONG_PTR)proc->UniqueProcessId == processId) {
                info->ThreadCount = proc->NumberOfThreads;
                info->HandleCount = proc->HandleCount;
                // Note: DirectoryTableBase, Token, ObjectTable, VadRoot cannot be
                // reliably obtained from usermode - offsets vary by Windows version
                // These fields are left as 0 unless read from kernel
                break;
            }
            if (proc->NextEntryOffset == 0) break;
            proc = (SYSTEM_PROCESS_INFORMATION*)((PUCHAR)proc + proc->NextEntryOffset);
        }
        LocalFree(procBuffer);
    }
    
    // Get first thread's KTHREAD
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te = { .dwSize = sizeof(te) };
        if (Thread32First(hSnapshot, &te)) {
            do {
                if (te.th32OwnerProcessID == processId) {
                    info->KThread = Core_GetKThread(processId, te.th32ThreadID);
                    break;
                }
            } while (Thread32Next(hSnapshot, &te));
        }
        CloseHandle(hSnapshot);
    }
    
    return true;
}

//
// Kernel Driver Enumeration Functions
//

#define RVATOVA(base, offset) ((PVOID)((PUCHAR)(base) + (ULONG_PTR)(offset)))

PVOID Core_GetSystemInformation(SYSTEM_INFORMATION_CLASS infoClass) {
    NTSTATUS status;
    PVOID buffer = NULL;
    ULONG bufferSize = 0;
    ULONG returnLength = 0;
    
    // First call to get required size
    status = NtQuerySystemInformation(infoClass, NULL, 0, &returnLength);
    if (returnLength == 0) {
        returnLength = 0x10000; // Default 64KB
    }
    
    // Allocate with some extra space
    bufferSize = returnLength + 0x1000;
    buffer = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bufferSize);
    if (!buffer) return NULL;
    
    // Retry with progressively larger buffers
    for (int i = 0; i < 5; i++) {
        status = NtQuerySystemInformation(infoClass, buffer, bufferSize, &returnLength);
        if (status == 0) {
            return buffer;
        }
        if (status == 0xC0000004L) { // STATUS_INFO_LENGTH_MISMATCH
            LocalFree(buffer);
            bufferSize = returnLength + 0x1000;
            buffer = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, bufferSize);
            if (!buffer) return NULL;
        } else {
            break;
        }
    }
    
    if (buffer) LocalFree(buffer);
    return NULL;
}

bool Core_GetKernelImageInfo(uint64_t* imageBase, uint32_t* imageSize, char* name, size_t nameSize) {
    PRTL_PROCESS_MODULES info = (PRTL_PROCESS_MODULES)Core_GetSystemInformation((SYSTEM_INFORMATION_CLASS)11);
    if (!info || info->NumberOfModules == 0) {
        if (info) LocalFree(info);
        return false;
    }
    
    PRTL_PROCESS_MODULE_INFORMATION module = &info->Modules[0];
    
    if (imageBase) *imageBase = (uint64_t)module->ImageBase;
    if (imageSize) *imageSize = module->ImageSize;
    if (name && nameSize > 0) {
        strncpy(name, (char*)(module->FullPathName + module->OffsetToFileName), nameSize - 1);
        name[nameSize - 1] = '\0';
    }
    
    LocalFree(info);
    return true;
}

bool Core_RefreshKernelDrivers(KERNEL_DRIVER_LIST* list) {
    if (!list) return false;
    
    PRTL_PROCESS_MODULES info = (PRTL_PROCESS_MODULES)Core_GetSystemInformation((SYSTEM_INFORMATION_CLASS)11);
    if (!info || info->NumberOfModules == 0) {
        if (info) LocalFree(info);
        return false;
    }
    
    // Allocate/reallocate driver list
    if (list->Drivers == NULL) {
        // First allocation - use malloc
        list->Capacity = info->NumberOfModules + 32;
        list->Drivers = (KERNEL_DRIVER_INFO*)malloc(list->Capacity * sizeof(KERNEL_DRIVER_INFO));
        if (!list->Drivers) {
            list->Capacity = 0;
            LocalFree(info);
            return false;
        }
    } else if (list->Capacity < info->NumberOfModules) {
        // Reallocation needed
        list->Capacity = info->NumberOfModules + 32;
        KERNEL_DRIVER_INFO* newDrivers = (KERNEL_DRIVER_INFO*)realloc(list->Drivers, 
            list->Capacity * sizeof(KERNEL_DRIVER_INFO));
        if (!newDrivers) {
            LocalFree(info);
            return false;
        }
        list->Drivers = newDrivers;
    }
    
    list->Count = 0;
    
    for (ULONG i = 0; i < info->NumberOfModules; i++) {
        PRTL_PROCESS_MODULE_INFORMATION module = &info->Modules[i];
        KERNEL_DRIVER_INFO* driver = &list->Drivers[list->Count];
        
        memset(driver, 0, sizeof(KERNEL_DRIVER_INFO));
        
        // Copy name (from offset)
        strncpy(driver->Name, (char*)(module->FullPathName + module->OffsetToFileName), 
            sizeof(driver->Name) - 1);
        
        // Copy full path
        strncpy(driver->FullPath, (char*)module->FullPathName, sizeof(driver->FullPath) - 1);
        
        driver->ImageBase = (uint64_t)module->ImageBase;
        driver->ImageSize = module->ImageSize;
        driver->LoadOrder = module->LoadOrderIndex;
        driver->OffsetToFileName = module->OffsetToFileName;
        driver->Valid = true;
        
        // First module is always ntoskrnl
        if (i == 0) {
            list->NtoskrnlBase = driver->ImageBase;
            list->NtoskrnlSize = driver->ImageSize;
            strncpy(list->NtoskrnlName, driver->Name, sizeof(list->NtoskrnlName) - 1);
        }
        
        list->Count++;
    }
    
    list->LastUpdate = Core_GetTickCount64();
    LocalFree(info);
    return true;
}

//
// PE Parsing and Export Enumeration
//

uint32_t Core_LdrGetProcAddress(PVOID image, const char* name) {
    if (!image || !name) return 0;
    
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)image;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)RVATOVA(image, dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) return 0;
    
    DWORD exportAddr = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    
    if (exportAddr == 0) return 0;
    
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)RVATOVA(image, exportAddr);
    
    if (exportDir->AddressOfFunctions == 0 ||
        exportDir->AddressOfNameOrdinals == 0 ||
        exportDir->AddressOfNames == 0) {
        return 0;
    }
    
    PDWORD addrOfFunctions = (PDWORD)RVATOVA(image, exportDir->AddressOfFunctions);
    PWORD addrOfOrdinals = (PWORD)RVATOVA(image, exportDir->AddressOfNameOrdinals);
    PDWORD addrOfNames = (PDWORD)RVATOVA(image, exportDir->AddressOfNames);
    
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        const char* exportName = (const char*)RVATOVA(image, addrOfNames[i]);
        
        if (strcmp(exportName, name) == 0) {
            DWORD addr = addrOfFunctions[addrOfOrdinals[i]];
            
            // Check for forwarder
            if (addr > exportAddr && addr < exportAddr + exportSize) {
                return 0; // This is a forwarder
            }
            
            return addr;
        }
    }
    
    return 0;
}

bool Core_GetSyscallNumber(const char* procName, uint32_t* syscallNumber) {
    if (!procName || !syscallNumber) return false;
    
    HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
    if (!hNtdll) return false;
    
    PUCHAR addr = (PUCHAR)GetProcAddress(hNtdll, procName);
    if (!addr) return false;
    
    // Check for syscall pattern: mov eax, XXXXXXXX at offset 3 or 4
    // Windows 10+ pattern
    if (*(addr + 3) == 0xB8) {
        *syscallNumber = *(PDWORD)(addr + 4);
        return true;
    }
    // Alternative pattern
    if (*(addr + 4) == 0xB8) {
        *syscallNumber = *(PDWORD)(addr + 5);
        return true;
    }
    
    return false;
}

bool Core_MapKernelImage(const char* systemPath, PVOID* mappedImage, uint32_t* imageSize) {
    if (!systemPath || !mappedImage || !imageSize) return false;
    
    char fullPath[MAX_PATH];
    char systemRoot[MAX_PATH];
    
    // Convert \SystemRoot\ or \??\ paths to full path
    if (strncmp(systemPath, "\\SystemRoot\\", 12) == 0) {
        GetWindowsDirectoryA(systemRoot, MAX_PATH);
        snprintf(fullPath, MAX_PATH, "%s\\%s", systemRoot, systemPath + 12);
    } else if (strncmp(systemPath, "\\??\\", 4) == 0) {
        strncpy(fullPath, systemPath + 4, MAX_PATH - 1);
    } else {
        strncpy(fullPath, systemPath, MAX_PATH - 1);
    }
    fullPath[MAX_PATH - 1] = '\0';
    
    // Open and read the file
    HANDLE hFile = CreateFileA(fullPath, GENERIC_READ, 
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL, OPEN_EXISTING, 0, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE) {
        // Try with system32 prefix
        GetSystemDirectoryA(fullPath, MAX_PATH);
        strncat(fullPath, "\\", MAX_PATH - strlen(fullPath) - 1);
        
        // Extract just filename from path
        const char* fileName = strrchr(systemPath, '\\');
        if (fileName) fileName++;
        else fileName = systemPath;
        
        strncat(fullPath, fileName, MAX_PATH - strlen(fullPath) - 1);
        
        hFile = CreateFileA(fullPath, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, 0, NULL);
    }
    
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD fileSizeHigh = 0;
    DWORD fileSize = GetFileSize(hFile, &fileSizeHigh);
    if (fileSize == 0 || fileSizeHigh != 0) {
        CloseHandle(hFile);
        return false;
    }
    
    PVOID fileData = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, fileSize);
    if (!fileData) {
        CloseHandle(hFile);
        return false;
    }
    
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, fileData, fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        LocalFree(fileData);
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    
    // Map PE sections
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)fileData;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        LocalFree(fileData);
        return false;
    }
    
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)RVATOVA(fileData, dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) {
        LocalFree(fileData);
        return false;
    }
    
    DWORD mappedSize = ntHeaders->OptionalHeader.SizeOfImage;
    PVOID image = LocalAlloc(LMEM_FIXED | LMEM_ZEROINIT, mappedSize);
    if (!image) {
        LocalFree(fileData);
        return false;
    }
    
    // Copy headers
    memcpy(image, fileData, ntHeaders->OptionalHeader.SizeOfHeaders);
    
    // Copy sections
    PIMAGE_SECTION_HEADER section = (PIMAGE_SECTION_HEADER)RVATOVA(
        &ntHeaders->OptionalHeader, ntHeaders->FileHeader.SizeOfOptionalHeader);
    
    for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
        if (section->SizeOfRawData > 0) {
            DWORD copySize = min(section->SizeOfRawData, section->Misc.VirtualSize);
            if (copySize > 0 && section->PointerToRawData + copySize <= fileSize) {
                memcpy(RVATOVA(image, section->VirtualAddress),
                    RVATOVA(fileData, section->PointerToRawData),
                    copySize);
            }
        }
        section++;
    }
    
    LocalFree(fileData);
    
    *mappedImage = image;
    *imageSize = mappedSize;
    return true;
}

bool Core_RefreshKernelExports(KERNEL_EXPORT_LIST* list, KERNEL_DRIVER_LIST* drivers, uint32_t driverIndex) {
    if (!list || !drivers || !drivers->Drivers || drivers->Count == 0) return false;
    if (driverIndex >= drivers->Count) return false;
    
    KERNEL_DRIVER_INFO* targetDriver = &drivers->Drivers[driverIndex];
    
    // Get driver base address
    list->NtoskrnlBase = targetDriver->ImageBase;
    
    // Free previous mapped image if any
    if (list->MappedImage) {
        LocalFree(list->MappedImage);
        list->MappedImage = NULL;
    }
    
    // Map the driver image
    if (!Core_MapKernelImage(targetDriver->FullPath, 
        &list->MappedImage, &list->MappedImageSize)) {
        return false;
    }
    
    PVOID image = list->MappedImage;
    PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)image;
    PIMAGE_NT_HEADERS ntHeaders = (PIMAGE_NT_HEADERS)RVATOVA(image, dosHeader->e_lfanew);
    
    DWORD exportAddr = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
    DWORD exportSize = ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
    
    if (exportAddr == 0) return false;
    
    PIMAGE_EXPORT_DIRECTORY exportDir = (PIMAGE_EXPORT_DIRECTORY)RVATOVA(image, exportAddr);
    
    if (exportDir->NumberOfNames == 0) return false;
    
    // Allocate export list
    if (list->Exports == NULL) {
        // First allocation
        list->Capacity = exportDir->NumberOfNames + 128;
        list->Exports = (KERNEL_EXPORT_INFO*)malloc(list->Capacity * sizeof(KERNEL_EXPORT_INFO));
        if (!list->Exports) {
            list->Capacity = 0;
            return false;
        }
    } else if (list->Capacity < exportDir->NumberOfNames) {
        // Reallocation needed
        list->Capacity = exportDir->NumberOfNames + 128;
        KERNEL_EXPORT_INFO* newExports = (KERNEL_EXPORT_INFO*)realloc(list->Exports,
            list->Capacity * sizeof(KERNEL_EXPORT_INFO));
        if (!newExports) return false;
        list->Exports = newExports;
    }
    
    PDWORD addrOfFunctions = (PDWORD)RVATOVA(image, exportDir->AddressOfFunctions);
    PWORD addrOfOrdinals = (PWORD)RVATOVA(image, exportDir->AddressOfNameOrdinals);
    PDWORD addrOfNames = (PDWORD)RVATOVA(image, exportDir->AddressOfNames);
    
    list->Count = 0;
    
    for (DWORD i = 0; i < exportDir->NumberOfNames; i++) {
        KERNEL_EXPORT_INFO* exp = &list->Exports[list->Count];
        memset(exp, 0, sizeof(KERNEL_EXPORT_INFO));
        
        const char* name = (const char*)RVATOVA(image, addrOfNames[i]);
        strncpy(exp->Name, name, sizeof(exp->Name) - 1);
        
        WORD ordinal = addrOfOrdinals[i];
        exp->Ordinal = ordinal + (DWORD)exportDir->Base;
        exp->RVA = addrOfFunctions[ordinal];
        
        // Check if it's a forwarder
        if (exp->RVA > exportAddr && exp->RVA < exportAddr + exportSize) {
            exp->IsForwarder = true;
            const char* forwarder = (const char*)RVATOVA(image, exp->RVA);
            strncpy(exp->ForwarderName, forwarder, sizeof(exp->ForwarderName) - 1);
            exp->KernelAddress = 0;
        } else {
            exp->IsForwarder = false;
            exp->KernelAddress = list->NtoskrnlBase + exp->RVA;
        }
        
        list->Count++;
    }
    
    list->LastUpdate = Core_GetTickCount64();
    return true;
}

uint64_t Core_GetKernelProcAddress(KERNEL_EXPORT_LIST* exports, const char* procName) {
    if (!exports || !procName || exports->Count == 0) return 0;
    
    for (uint32_t i = 0; i < exports->Count; i++) {
        if (strcmp(exports->Exports[i].Name, procName) == 0) {
            return exports->Exports[i].KernelAddress;
        }
    }
    
    return 0;
}

void Core_FreeKernelExports(KERNEL_EXPORT_LIST* list) {
    if (!list) return;
    
    if (list->Exports) {
        free(list->Exports);
        list->Exports = NULL;
    }
    if (list->MappedImage) {
        LocalFree(list->MappedImage);
        list->MappedImage = NULL;
    }
    list->Count = 0;
    list->Capacity = 0;
}

//
// Find important kernel pointers by scanning and exports
//

bool Core_FindKernelPointers(KERNEL_POINTERS* ptrs, KERNEL_EXPORT_LIST* exports, KERNEL_DRIVER_LIST* drivers) {
    if (!ptrs) return false;
    
    memset(ptrs, 0, sizeof(KERNEL_POINTERS));
    
    // First try to find pointers via exports (most reliable)
    if (exports && exports->Count > 0 && exports->MappedImage) {
        // These are exported symbols we can find directly
        ptrs->PsLoadedModuleList = Core_GetKernelProcAddress(exports, "PsLoadedModuleList");
        ptrs->PsActiveProcessHead = Core_GetKernelProcAddress(exports, "PsActiveProcessHead");
        ptrs->ObTypeIndexTable = Core_GetKernelProcAddress(exports, "ObTypeIndexTable");
        ptrs->HalDispatchTable = Core_GetKernelProcAddress(exports, "HalDispatchTable");
        ptrs->HalPrivateDispatchTable = Core_GetKernelProcAddress(exports, "HalPrivateDispatchTable");
        ptrs->KeServiceDescriptorTable = Core_GetKernelProcAddress(exports, "KeServiceDescriptorTable");
        ptrs->KeServiceDescriptorTableShadow = Core_GetKernelProcAddress(exports, "KeServiceDescriptorTableShadow");
        
        // For unexported symbols, we need to scan the kernel image
        PVOID image = exports->MappedImage;
        uint64_t kernelBase = exports->NtoskrnlBase;
        
        if (image && kernelBase) {
            PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)image;
            PIMAGE_NT_HEADERS64 ntHeader = (PIMAGE_NT_HEADERS64)RVATOVA(image, dosHeader->e_lfanew);
            PBYTE mappedBase = (PBYTE)image;
            
            // Find KiSystemCall64/32 by signature
            // Pattern: 0f 01 f8 65 48 89 24 25 10 00 00 00 65 48 8b 24
            DWORD kiSystemCall32Offset = 0;
            DWORD kiSystemCall64Offset = 0;
            
            for (DWORD i = ntHeader->OptionalHeader.BaseOfCode; 
                 i < ntHeader->OptionalHeader.SizeOfCode - 0x20; 
                 i += 0x20) {
                PULONG64 data = (PULONG64)(mappedBase + i);
                // Signature for KiSystemCall64/32 entry
                if (data[0] == 0x2524894865f8010f && data[1] == 0x248b486500000010) {
                    if (kiSystemCall32Offset == 0) {
                        kiSystemCall32Offset = i;
                    } else {
                        kiSystemCall64Offset = i;
                        break;
                    }
                }
            }
            
            if (kiSystemCall32Offset) {
                ptrs->KiSystemCall32 = kernelBase + kiSystemCall32Offset;
            }
            if (kiSystemCall64Offset) {
                ptrs->KiSystemCall64 = kernelBase + kiSystemCall64Offset;
            }
            
            // If SSDT wasn't exported, try to find it via lea instruction in KiSystemCall64
            if (ptrs->KeServiceDescriptorTable == 0 && kiSystemCall64Offset != 0) {
                for (DWORD i = kiSystemCall64Offset; i < kiSystemCall64Offset + 0x1000 && i < ntHeader->OptionalHeader.SizeOfImage - 10; i++) {
                    // Look for: 4c 8d 15 XX XX XX XX  (lea r10, [rip+XX])
                    //           4c 8d 1d XX XX XX XX  (lea r11, [rip+XX])
                    if (mappedBase[i] == 0x4c && mappedBase[i + 1] == 0x8d && mappedBase[i + 2] == 0x15) {
                        if (mappedBase[i + 7] == 0x4c && mappedBase[i + 8] == 0x8d && mappedBase[i + 9] == 0x1d) {
                            // Found the pattern - extract RIP-relative offset
                            LONG ripOffset;
                            memcpy(&ripOffset, &mappedBase[i + 3], sizeof(ripOffset));
                            ptrs->KeServiceDescriptorTable = kernelBase + i + 7 + ripOffset;
                            
                            // Also get shadow SSDT
                            memcpy(&ripOffset, &mappedBase[i + 10], sizeof(ripOffset));
                            ptrs->KeServiceDescriptorTableShadow = kernelBase + i + 14 + ripOffset;
                            break;
                        }
                    }
                }
            }
        }
    }
    
    ptrs->Valid = (ptrs->KeServiceDescriptorTable != 0 || ptrs->PsLoadedModuleList != 0);
    ptrs->LastUpdate = Core_GetTickCount64();
    
    return ptrs->Valid;
}
