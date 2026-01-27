#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winternl.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "psapi.lib")

// NT functions not in winternl.h
NTSTATUS NTAPI NtOpenSection(
    PHANDLE SectionHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes
);

#ifdef __cplusplus
extern "C" {
#endif

    //
    // Constants
    //

#define PAGE_SIZE 0x1000
#define PAGE_SHIFT 12
#define SUPERFETCH_VERSION 45
#define SUPERFETCH_MAGIC 'kuhC'

#define SYS_SuperfetchInformation 79
#define SYS_MemoryListInformation 80
#define SYS_PageFileInformation 18
#define SYS_PerformanceInformation 2
#define SYS_ProcessInformation 5
#define SYS_ProcessorPerformanceInformation 8
#define SYS_InterruptInformation 23
#define SYS_ExceptionInformation 33
#define SYS_FileCacheInformation 21
#define SYS_FileCacheInformationEx 81
#define SYS_PoolTagInformation 22
#define SYS_HandleInformation 16
#define SYS_ExtendedHandleInformation 64
#define SYS_ObjectInformation 17
#define SYS_SessionProcessInformation 53
#define SYS_CombinePhysicalMemoryInformation 130
#define SYS_MemoryListInformationEx 104
#define SYS_StoreInformation 180
#define SYS_MemoryTopologyInformation 78
#define SYS_NumaProximityNodeInformation 89
#define SYS_ProcessorIdleCycleTimeInformation 83
#define SYS_CodeIntegrityInformation 103

//
// Superfetch Information Classes (Complete)
//

    typedef enum _SUPERFETCH_INFORMATION_CLASS {
        SuperfetchRetrieveTrace = 1,
        SuperfetchSystemParameters = 2,
        SuperfetchLogEvent = 3,
        SuperfetchGenerateTrace = 4,
        SuperfetchPrefetch = 5,
        SuperfetchPfnQuery = 6,
        SuperfetchPfnSetPriority = 7,
        SuperfetchPrivSourceQuery = 8,
        SuperfetchSequenceNumberQuery = 9,
        SuperfetchScenarioPhase = 10,
        SuperfetchWorkerPriority = 11,
        SuperfetchScenarioQuery = 12,
        SuperfetchScenarioPrefetch = 13,
        SuperfetchRobustnessControl = 14,
        SuperfetchTimeControl = 15,
        SuperfetchMemoryListQuery = 16,
        SuperfetchMemoryRangesQuery = 17,
        SuperfetchTracingControl = 18,
        SuperfetchTrimWhileAgingControl = 19,
        SuperfetchInformationMax = 20
    } SUPERFETCH_INFORMATION_CLASS;

    //
    // Memory List Information (must be defined before PF_PFN_PRIO_REQUEST)
    //

    typedef struct _SYS_MEMORY_LIST_INFORMATION {
        SIZE_T ZeroPageCount;
        SIZE_T FreePageCount;
        SIZE_T ModifiedPageCount;
        SIZE_T ModifiedNoWritePageCount;
        SIZE_T BadPageCount;
        SIZE_T PageCountByPriority[8];
        SIZE_T RepurposedPagesByPriority[8];
        ULONG_PTR ModifiedPageCountPageFile;
    } SYS_MEMORY_LIST_INFORMATION, * PSYS_MEMORY_LIST_INFORMATION;

    //
    // Core Structures
    //

#pragma pack(push, 8)
    typedef struct _SUPERFETCH_INFORMATION {
        ULONG Version;
        ULONG Magic;
        SUPERFETCH_INFORMATION_CLASS InfoClass;
        PVOID Data;
        ULONG Length;
    } SUPERFETCH_INFORMATION, * PSUPERFETCH_INFORMATION;
#pragma pack(pop)

    typedef struct _PF_MEMORY_RANGE_INFO {
        ULONG Version;
        ULONG RangeCount;
        struct {
            ULONG_PTR StartPage;
            ULONG_PTR PageCount;
        } Ranges[256];
    } PF_MEMORY_RANGE_INFO, * PPF_MEMORY_RANGE_INFO;

    //
    // PFN/Physical Memory Structures (for VA->PA translation)
    //

    typedef struct _MEMORY_FRAME_INFORMATION {
        ULONGLONG UseDescription : 4;   // MMLISTS_USE enum
        ULONGLONG ListDescription : 3;   // MMPFNLIST_* 
        ULONGLONG Reserved0 : 1;
        ULONGLONG Pinned : 1;
        ULONGLONG DontUse : 48;
        ULONGLONG Priority : 3;
        ULONGLONG Reserved : 4;
    } MEMORY_FRAME_INFORMATION;

    typedef struct _FILEOFFSET_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG Offset : 48;  // File offset in pages
        ULONGLONG Reserved : 7;
    } FILEOFFSET_INFORMATION;

    typedef struct _PAGEDIR_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG PageDirectoryBase : 48;
        ULONGLONG Reserved : 7;
    } PAGEDIR_INFORMATION;

    typedef struct _UNIQUE_PROCESS_INFORMATION {
        ULONGLONG DontUse : 9;
        ULONGLONG UniqueProcessKey : 48;
        ULONGLONG Reserved : 7;
    } UNIQUE_PROCESS_INFORMATION;

    typedef struct _MMPFN_IDENTITY {
        union {
            MEMORY_FRAME_INFORMATION   e1;
            FILEOFFSET_INFORMATION     e2;
            PAGEDIR_INFORMATION        e3;
            UNIQUE_PROCESS_INFORMATION e4;
        } u1;
        SIZE_T PageFrameIndex;
        union {
            struct {
                ULONG Image : 1;
                ULONG Mismatch : 1;
            } e1;
            PVOID FileObject;
            PVOID UniqueFileObjectKey;
            PVOID ProtoPteAddress;
            PVOID VirtualAddress;
        } u2;
    } MMPFN_IDENTITY, * PMMPFN_IDENTITY;

    typedef struct _PF_PFN_PRIO_REQUEST {
        ULONG Version;
        ULONG RequestFlags;
        SIZE_T PfnCount;
        SYS_MEMORY_LIST_INFORMATION MemInfo;
        MMPFN_IDENTITY PageData[1];  // Variable size
    } PF_PFN_PRIO_REQUEST, * PPF_PFN_PRIO_REQUEST;

    //
    // Physical Memory Range Structures (V2)
    //

    typedef struct _PF_PHYSICAL_MEMORY_RANGE {
        ULONG_PTR BasePfn;
        ULONG_PTR PageCount;
    } PF_PHYSICAL_MEMORY_RANGE, * PPF_PHYSICAL_MEMORY_RANGE;

    typedef struct _PF_MEMORY_RANGE_INFO_V2 {
        ULONG Version;
        ULONG Flags;
        ULONG RangeCount;
        PF_PHYSICAL_MEMORY_RANGE Ranges[1];  // Variable size
    } PF_MEMORY_RANGE_INFO_V2, * PPF_MEMORY_RANGE_INFO_V2;

    //
    // Per-Process Superfetch Working Set
    //

    typedef struct _PF_PRIVSOURCE_INFO {
        ULONG DbInfo;
        PVOID EProcess;
        WCHAR ImageName[16];
        ULONG_PTR WssPages;      // Working Set Shareable
        ULONG_PTR WsPages;       // Total Working Set
        ULONG_PTR WsPrivate;     // Private Working Set
        ULONG_PTR WsShared;      // Shared Working Set
        ULONG SessionId;
        LONG ProcessId;
        ULONG WsIndex;
        ULONG Flags;
        LARGE_INTEGER CreateTime;
    } PF_PRIVSOURCE_INFO, * PPF_PRIVSOURCE_INFO;

    typedef struct _PF_PRIVSOURCE_QUERY {
        ULONG Version;
        ULONG Flags;
        ULONG InfoCount;
        PF_PRIVSOURCE_INFO Sources[1];
    } PF_PRIVSOURCE_QUERY, * PPF_PRIVSOURCE_QUERY;

    //
    // System Performance Information (Custom - avoids SDK conflict)
    //

    typedef struct _SYS_PERFORMANCE_INFO {
        LARGE_INTEGER IdleProcessTime;
        LARGE_INTEGER IoReadTransferCount;
        LARGE_INTEGER IoWriteTransferCount;
        LARGE_INTEGER IoOtherTransferCount;
        ULONG IoReadOperationCount;
        ULONG IoWriteOperationCount;
        ULONG IoOtherOperationCount;
        ULONG AvailablePages;
        ULONG CommittedPages;
        ULONG CommitLimit;
        ULONG PeakCommitment;
        ULONG PageFaultCount;
        ULONG CopyOnWriteCount;
        ULONG TransitionCount;
        ULONG CacheTransitionCount;
        ULONG DemandZeroCount;
        ULONG PageReadCount;
        ULONG PageReadIoCount;
        ULONG CacheReadCount;
        ULONG CacheIoCount;
        ULONG DirtyPagesWriteCount;
        ULONG DirtyWriteIoCount;
        ULONG MappedPagesWriteCount;
        ULONG MappedWriteIoCount;
        ULONG PagedPoolPages;
        ULONG NonPagedPoolPages;
        ULONG PagedPoolAllocs;
        ULONG PagedPoolFrees;
        ULONG NonPagedPoolAllocs;
        ULONG NonPagedPoolFrees;
        ULONG FreeSystemPtes;
        ULONG ResidentSystemCodePage;
        ULONG TotalSystemDriverPages;
        ULONG TotalSystemCodePages;
        ULONG NonPagedPoolLookasideHits;
        ULONG PagedPoolLookasideHits;
        ULONG AvailablePagedPoolPages;
        ULONG ResidentSystemCachePage;
        ULONG ResidentPagedPoolPage;
        ULONG ResidentSystemDriverPage;
        ULONG CcFastReadNoWait;
        ULONG CcFastReadWait;
        ULONG CcFastReadResourceMiss;
        ULONG CcFastReadNotPossible;
        ULONG CcFastMdlReadNoWait;
        ULONG CcFastMdlReadWait;
        ULONG CcFastMdlReadResourceMiss;
        ULONG CcFastMdlReadNotPossible;
        ULONG CcMapDataNoWait;
        ULONG CcMapDataWait;
        ULONG CcMapDataNoWaitMiss;
        ULONG CcMapDataWaitMiss;
        ULONG CcPinMappedDataCount;
        ULONG CcPinReadNoWait;
        ULONG CcPinReadWait;
        ULONG CcPinReadNoWaitMiss;
        ULONG CcPinReadWaitMiss;
        ULONG CcCopyReadNoWait;
        ULONG CcCopyReadWait;
        ULONG CcCopyReadNoWaitMiss;
        ULONG CcCopyReadWaitMiss;
        ULONG CcMdlReadNoWait;
        ULONG CcMdlReadWait;
        ULONG CcMdlReadNoWaitMiss;
        ULONG CcMdlReadWaitMiss;
        ULONG CcReadAheadIos;
        ULONG CcLazyWriteIos;
        ULONG CcLazyWritePages;
        ULONG CcDataFlushes;
        ULONG CcDataPages;
        ULONG ContextSwitches;
        ULONG FirstLevelTbFills;
        ULONG SecondLevelTbFills;
        ULONG SystemCalls;
        ULONGLONG CcTotalDirtyPages;
        ULONGLONG CcDirtyPageThreshold;
        LONGLONG ResidentAvailablePages;
        ULONGLONG SharedCommittedPages;
    } SYS_PERFORMANCE_INFO, * PSYS_PERFORMANCE_INFO;

    //
    // File Cache Information (Custom - avoids SDK conflict)
    //

    typedef struct _SYS_FILECACHE_INFO {
        SIZE_T CurrentSize;
        SIZE_T PeakSize;
        ULONG PageFaultCount;
        SIZE_T MinimumWorkingSet;
        SIZE_T MaximumWorkingSet;
        SIZE_T CurrentSizeIncludingTransitionInPages;
        SIZE_T PeakSizeIncludingTransitionInPages;
        ULONG TransitionRePurposeCount;
        ULONG Flags;
    } SYS_FILECACHE_INFO, * PSYS_FILECACHE_INFO;

    //
    // Interrupt Information (Custom - avoids SDK conflict)
    //

    typedef struct _SYS_INTERRUPT_INFO {
        ULONG ContextSwitches;
        ULONG DpcCount;
        ULONG DpcRate;
        ULONG TimeIncrement;
        ULONG DpcBypassCount;
        ULONG ApcBypassCount;
    } SYS_INTERRUPT_INFO, * PSYS_INTERRUPT_INFO;

    //
    // Processor Performance Information (Custom - avoids SDK conflict)
    //

    typedef struct _SYS_PROCESSOR_PERF_INFO {
        LARGE_INTEGER IdleTime;
        LARGE_INTEGER KernelTime;
        LARGE_INTEGER UserTime;
        LARGE_INTEGER DpcTime;
        LARGE_INTEGER InterruptTime;
        ULONG InterruptCount;
    } SYS_PROCESSOR_PERF_INFO, * PSYS_PROCESSOR_PERF_INFO;

    //
    // Handle/Object Information (Custom)
    //

    // Basic handle entry (for SystemHandleInformation - class 16)
    typedef struct _SYS_HANDLE_ENTRY {
        USHORT UniqueProcessId;
        USHORT CreatorBackTraceIndex;
        UCHAR ObjectTypeIndex;
        UCHAR HandleAttributes;
        USHORT HandleValue;
        PVOID Object;
        ULONG GrantedAccess;
    } SYS_HANDLE_ENTRY, * PSYS_HANDLE_ENTRY;

    typedef struct _SYS_HANDLE_INFO {
        ULONG NumberOfHandles;
        SYS_HANDLE_ENTRY Handles[1];
    } SYS_HANDLE_INFO, * PSYS_HANDLE_INFO;

    // Extended handle entry (for SystemExtendedHandleInformation - class 64)
    typedef struct _SYS_HANDLE_ENTRY_EX {
        PVOID Object;
        ULONG_PTR UniqueProcessId;
        ULONG_PTR HandleValue;
        ULONG GrantedAccess;
        USHORT CreatorBackTraceIndex;
        USHORT ObjectTypeIndex;
        ULONG HandleAttributes;
        ULONG Reserved;
    } SYS_HANDLE_ENTRY_EX, * PSYS_HANDLE_ENTRY_EX;

    typedef struct _SYS_HANDLE_INFO_EX {
        ULONG_PTR NumberOfHandles;
        ULONG_PTR Reserved;
        SYS_HANDLE_ENTRY_EX Handles[1];
    } SYS_HANDLE_INFO_EX, * PSYS_HANDLE_INFO_EX;

#define OBJECT_HEADER_TO_BODY_OFFSET 0x30

    typedef struct _OBJECT_HEADER {
        LONGLONG PointerCount;
        union {
            LONGLONG HandleCount;
            PVOID NextToFree;
        };
        PVOID Lock;
        UCHAR TypeIndex;
        union {
            UCHAR TraceFlags;
            struct {
                UCHAR DbgRefTrace : 1;
                UCHAR DbgTracePermanent : 1;
            };
        };
        UCHAR InfoMask;
        union {
            UCHAR Flags;
            struct {
                UCHAR NewObject : 1;
                UCHAR KernelObject : 1;
                UCHAR KernelOnlyAccess : 1;
                UCHAR ExclusiveObject : 1;
                UCHAR PermanentObject : 1;
                UCHAR DefaultSecurityQuota : 1;
                UCHAR SingleHandleEntry : 1;
                UCHAR DeletedInline : 1;
            };
        };
        ULONG Reserved;
        union {
            PVOID ObjectCreateInfo;
            PVOID QuotaBlockCharged;
        };
        PVOID SecurityDescriptor;
    } OBJECT_HEADER, * POBJECT_HEADER;

    typedef struct _PHYSICALMEMORY_OBJECT_INFO {
        uint64_t ObjectBodyAddress;
        uint64_t ObjectHeaderAddress;
        uint64_t HandleValue;
        uint32_t HandleCount;
        bool KernelOnlyAccess;
        bool Found;
    } PHYSICALMEMORY_OBJECT_INFO, * PPHYSICALMEMORY_OBJECT_INFO;

    //
    // Pool Tag Information (Custom)
    //

    typedef struct _SYS_POOLTAG {
        union {
            UCHAR Tag[4];
            ULONG TagUlong;
        };
        ULONG PagedAllocs;
        ULONG PagedFrees;
        SIZE_T PagedUsed;
        ULONG NonPagedAllocs;
        ULONG NonPagedFrees;
        SIZE_T NonPagedUsed;
    } SYS_POOLTAG, * PSYS_POOLTAG;

    typedef struct _SYS_POOLTAG_INFO {
        ULONG Count;
        SYS_POOLTAG TagInfo[1];
    } SYS_POOLTAG_INFO, * PSYS_POOLTAG_INFO;

    //
    // Memory Compression (Win10+)
    //

    typedef struct _SYSTEM_STORE_INFORMATION {
        ULONG Version;
        ULONG StoreInformationClass;
        PVOID Data;
        ULONG Length;
    } SYSTEM_STORE_INFORMATION, * PSYSTEM_STORE_INFORMATION;

    typedef struct _SM_MEM_COMPRESSION_INFO {
        ULONG Version;
        ULONG CompressionPid;
        ULONGLONG WorkingSetSize;
        ULONGLONG TotalDataCompressed;
        ULONGLONG TotalCompressedSize;
        ULONGLONG TotalUniqueDataCompressed;
    } SM_MEM_COMPRESSION_INFO, * PSM_MEM_COMPRESSION_INFO;

    //
    // System Code Integrity Information (Custom - avoids SDK conflict)
    //

    typedef struct _SYS_CODEINTEGRITY_INFO {
        ULONG Length;
        ULONG CodeIntegrityOptions;
    } SYS_CODEINTEGRITY_INFO, * PSYS_CODEINTEGRITY_INFO;

    //
    // NUMA Topology Information (Custom)
    //

    typedef struct _SYS_NUMA_INFO {
        ULONG HighestNodeNumber;
        ULONG Reserved;
        union {
            GROUP_AFFINITY ActiveProcessorsGroupAffinity[64];
            ULONGLONG AvailableMemory[64];
            ULONGLONG Pad[128];
        };
    } SYS_NUMA_INFO, * PSYS_NUMA_INFO;

    //
    // Pagefile Information (Custom)
    //

    typedef struct _SYS_PAGEFILE_INFO {
        ULONG NextEntryOffset;
        ULONG TotalSize;
        ULONG TotalInUse;
        ULONG PeakUsage;
        UNICODE_STRING PageFileName;
    } SYS_PAGEFILE_INFO, * PSYS_PAGEFILE_INFO;

    //
    // Cached Data Structures (for GUI refresh optimization)
    //

    typedef struct _MEMORY_STATS {
        // Memory lists
        uint64_t ZeroPageCount;
        uint64_t FreePageCount;
        uint64_t ModifiedPageCount;
        uint64_t ModifiedNoWritePageCount;
        uint64_t BadPageCount;
        uint64_t StandbyPageCount[8];
        uint64_t RepurposedCount[8];

        // Totals
        uint64_t TotalPhysical;
        uint64_t AvailablePhysical;
        uint64_t TotalStandby;
        uint64_t TotalFree;

        // Pagefile
        uint64_t PagefileTotal;
        uint64_t PagefileInUse;
        uint64_t PagefilePeak;

        // Compression (Win10+)
        uint64_t CompressedPages;
        uint64_t CompressionSavings;
        uint32_t CompressionRatio;

        // Pool Memory
        uint64_t PagedPoolPages;
        uint64_t NonPagedPoolPages;
        uint64_t PagedPoolAllocs;
        uint64_t NonPagedPoolAllocs;
        uint64_t PagedPoolFrees;
        uint64_t NonPagedPoolFrees;

        // File Cache
        uint64_t FileCacheSize;
        uint64_t FileCachePeak;
        uint32_t FileCachePageFaults;

        // System Counters
        uint64_t ContextSwitches;
        uint64_t SystemCalls;
        uint64_t PageFaults;
        uint64_t CopyOnWriteCount;
        uint64_t TransitionFaults;
        uint64_t DemandZeroFaults;

        // Commit
        uint64_t CommittedPages;
        uint64_t CommitLimit;
        uint64_t PeakCommitment;
        uint64_t SharedCommit;

        // Timing
        uint64_t LastUpdate;
        bool Valid;
    } MEMORY_STATS;

    //
    // Extended Process Info with Superfetch data
    //

    typedef struct _PROCESS_MEMORY_INFO {
        DWORD Pid;
        wchar_t Name[64];
        uint64_t WorkingSet;
        uint64_t PeakWorkingSet;
        uint64_t PrivateBytes;
        uint64_t PagefileUsage;
        uint32_t PageFaults;
        uint32_t Priority;
        // Superfetch extended info
        uint64_t WsShareable;      // Working set shareable
        uint64_t WsShared;         // Shared working set
        uint64_t WsPrivateSuper;   // Private from superfetch
        uint32_t SessionId;
        PVOID EProcess;            // Kernel EPROCESS pointer
        LARGE_INTEGER CreateTime;
        bool HasSuperfetchData;
        bool Selected;
    } PROCESS_MEMORY_INFO;

    typedef struct _PROCESS_LIST {
        PROCESS_MEMORY_INFO* Processes;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t TotalWorkingSet;
        uint64_t TotalPrivate;
        uint64_t LastUpdate;
        int SortColumn;
        bool SortAscending;
    } PROCESS_LIST;

    //
    // PFN Query Results (Virtual-to-Physical translation)
    //

    typedef struct _PFN_INFO {
        uint64_t VirtualAddress;
        uint64_t PhysicalAddress;
        uint64_t PageFrameIndex;
        uint32_t UseDescription;
        uint32_t ListDescription;
        uint32_t Priority;
        bool Pinned;
        bool Image;
        bool Valid;
    } PFN_INFO;

    typedef struct _PFN_QUERY_RESULTS {
        PFN_INFO* Pages;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t QueryTimeMs;
        uint64_t LastUpdate;
    } PFN_QUERY_RESULTS;

    //
    // Physical Memory Ranges
    //

    typedef struct _MEMORY_RANGE {
        uint64_t StartPage;
        uint64_t PageCount;
        uint64_t SizeMB;
    } MEMORY_RANGE;

    typedef struct _PHYSICAL_MEMORY_MAP {
        MEMORY_RANGE* Ranges;
        uint32_t RangeCount;
        uint64_t TotalPages;
        uint64_t TotalSizeGB;
        uint64_t LastUpdate;
    } PHYSICAL_MEMORY_MAP;

    //
    // Performance Stats
    //

    typedef struct _PERF_STATS {
        // Per-processor
        uint32_t ProcessorCount;
        uint64_t* IdleTime;
        uint64_t* KernelTime;
        uint64_t* UserTime;
        uint64_t* DpcTime;
        uint64_t* InterruptTime;
        uint32_t* InterruptCount;
        uint32_t* DpcCount;

        // System-wide
        uint64_t TotalIdleTime;
        uint64_t TotalKernelTime;
        uint64_t TotalUserTime;
        float CpuUsage;

        // I/O
        uint64_t IoReadBytes;
        uint64_t IoWriteBytes;
        uint64_t IoOtherBytes;
        uint32_t IoReadOps;
        uint32_t IoWriteOps;
        uint32_t IoOtherOps;

        uint64_t LastUpdate;
        bool Valid;
    } PERF_STATS;

    //
    // Pool Tag Information
    //

    typedef struct _POOLTAG_INFO {
        char Tag[5];
        uint32_t PagedAllocs;
        uint32_t PagedFrees;
        uint64_t PagedUsed;
        uint32_t NonPagedAllocs;
        uint32_t NonPagedFrees;
        uint64_t NonPagedUsed;
        uint64_t TotalUsed;
    } POOLTAG_INFO;

    typedef struct _POOLTAG_LIST {
        POOLTAG_INFO* Tags;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t TotalPagedUsed;
        uint64_t TotalNonPagedUsed;
        uint64_t LastUpdate;
    } POOLTAG_LIST;

    //
    // Handle Statistics
    //

    typedef struct _HANDLE_PROCESS_INFO {
        ULONG ProcessId;
        wchar_t ProcessName[64];
        uint32_t HandleCount;
        // Per-type breakdown for this process
        uint32_t FileHandles;
        uint32_t KeyHandles;
        uint32_t EventHandles;
        uint32_t MutantHandles;
        uint32_t SectionHandles;
        uint32_t ThreadHandles;
        uint32_t ProcessHandles;
        uint32_t TokenHandles;
        uint32_t OtherHandles;
    } HANDLE_PROCESS_INFO;

    // Detailed handle entry for display
    typedef struct _HANDLE_DETAIL {
        ULONG ProcessId;
        ULONG_PTR HandleValue;
        ULONG_PTR ObjectAddress;
        ULONG GrantedAccess;
        USHORT ObjectTypeIndex;
        char TypeName[32];
        wchar_t ObjectName[256];  // For files, keys, etc.
        bool NameResolved;
    } HANDLE_DETAIL;

    typedef struct _HANDLE_DETAIL_LIST {
        HANDLE_DETAIL* Handles;
        uint32_t Count;
        uint32_t Capacity;
        ULONG FilterPid;  // 0 = all processes
        uint64_t LastUpdate;
    } HANDLE_DETAIL_LIST;

    typedef struct _HANDLE_STATS {
        uint64_t TotalHandles;
        uint64_t UniqueProcesses;
        uint64_t HandlesByType[64];
        char TypeNames[64][32];
        uint32_t TypeCount;
        // All processes by handle count (dynamically allocated, sorted by count desc)
        HANDLE_PROCESS_INFO* Processes;
        uint32_t ProcessCount;
        uint32_t ProcessCapacity;
        uint64_t LastUpdate;
    } HANDLE_STATS;

    //
    // Prefetch Entries
    //

    typedef struct _PREFETCH_ENTRY {
        wchar_t Name[128];
        wchar_t CleanName[64];
        uint64_t LastAccess;
        uint32_t Size;
        uint32_t RunCount;
    } PREFETCH_ENTRY;

    typedef struct _PREFETCH_LIST {
        PREFETCH_ENTRY* Entries;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t LastUpdate;
    } PREFETCH_LIST;

    //
    // Kernel Driver Information
    //

    typedef struct _KERNEL_DRIVER_INFO {
        char Name[64];
        char FullPath[260];
        uint64_t ImageBase;
        uint32_t ImageSize;
        uint32_t LoadOrder;
        uint16_t OffsetToFileName;
        bool Valid;
    } KERNEL_DRIVER_INFO;

    typedef struct _KERNEL_DRIVER_LIST {
        KERNEL_DRIVER_INFO* Drivers;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t NtoskrnlBase;
        uint32_t NtoskrnlSize;
        char NtoskrnlName[64];
        uint64_t LastUpdate;
    } KERNEL_DRIVER_LIST;

    //
    // Ntoskrnl Export Information
    //

    typedef struct _KERNEL_EXPORT_INFO {
        char Name[128];
        uint64_t KernelAddress;
        uint32_t Ordinal;
        uint32_t RVA;
        bool IsForwarder;
        char ForwarderName[64];
    } KERNEL_EXPORT_INFO;

    typedef struct _KERNEL_EXPORT_LIST {
        KERNEL_EXPORT_INFO* Exports;
        uint32_t Count;
        uint32_t Capacity;
        uint64_t NtoskrnlBase;
        PVOID MappedImage;
        uint32_t MappedImageSize;
        uint64_t LastUpdate;
    } KERNEL_EXPORT_LIST;

    //
    // Important Kernel Pointers/Structures
    //

    typedef struct _KERNEL_POINTERS {
        uint64_t KeServiceDescriptorTable;      // SSDT
        uint64_t KeServiceDescriptorTableShadow; // Shadow SSDT (win32k)
        uint64_t KiSystemCall64;                // System call handler
        uint64_t KiSystemCall32;                // WoW64 system call handler
        uint64_t PsLoadedModuleList;            // Loaded modules list
        uint64_t PsActiveProcessHead;           // Active process list
        uint64_t ObTypeIndexTable;              // Object type table
        uint64_t HalDispatchTable;              // HAL dispatch table
        uint64_t HalPrivateDispatchTable;       // HAL private dispatch
        uint64_t KiDispatchInterrupt;           // Dispatch interrupt handler
        bool Valid;
        uint64_t LastUpdate;
    } KERNEL_POINTERS;

    //
    // RTL_PROCESS_MODULES for NtQuerySystemInformation class 11
    //

    typedef struct _RTL_PROCESS_MODULE_INFORMATION {
        HANDLE Section;
        PVOID MappedBase;
        PVOID ImageBase;
        ULONG ImageSize;
        ULONG Flags;
        USHORT LoadOrderIndex;
        USHORT InitOrderIndex;
        USHORT LoadCount;
        USHORT OffsetToFileName;
        UCHAR FullPathName[256];
    } RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;

    typedef struct _RTL_PROCESS_MODULES {
        ULONG NumberOfModules;
        RTL_PROCESS_MODULE_INFORMATION Modules[1];
    } RTL_PROCESS_MODULES, *PRTL_PROCESS_MODULES;

    //
    // Compression Stats (Win10+)
    //

    typedef struct _COMPRESSION_STATS {
        uint64_t CompressionPid;
        uint64_t WorkingSetSize;
        uint64_t TotalDataCompressed;
        uint64_t TotalCompressedSize;
        uint64_t CompressionSavings;
        float CompressionRatio;
        bool Available;
        uint64_t LastUpdate;
    } COMPRESSION_STATS;

    //
    // Global Application State
    //

    typedef struct _APP_STATE {
        // Cached data
        MEMORY_STATS MemStats;
        PROCESS_LIST ProcList;
        PHYSICAL_MEMORY_MAP PhysMap;
        PREFETCH_LIST PrefetchList;
        PERF_STATS PerfStats;
        POOLTAG_LIST PoolTags;
        HANDLE_STATS HandleStats;
        COMPRESSION_STATS CompressionStats;
        PFN_QUERY_RESULTS PfnResults;

        // Kernel driver and export data
        KERNEL_DRIVER_LIST DriverList;
        KERNEL_EXPORT_LIST ExportList;
        KERNEL_POINTERS KernelPtrs;

        // Superfetch process data
        PF_PRIVSOURCE_INFO* SuperfetchProcs;
        uint32_t SuperfetchProcCount;

        // Settings
        uint32_t RefreshIntervalMs;
        bool AutoRefresh;
        bool DarkMode;
        bool ShowGraphs;

        // History for graphs (ring buffers)
        float StandbyHistory[120];
        float FreeHistory[120];
        float ModifiedHistory[120];
        float CommitHistory[120];
        float CpuHistory[120];
        float IoHistory[120];
        int HistoryIndex;

        // UI State
        int SelectedTab;
        int SelectedProcess;
        char SearchFilter[256];
        bool ShowSettings;

        // Address translation
        char VaInputBuffer[32];
        uint64_t LastTranslatedVA;
        uint64_t LastTranslatedPA;
        bool TranslationValid;

        // Performance
        uint64_t LastFrameTime;
        float FrameTimeMs;
        uint32_t UpdateCount;
    } APP_STATE;

    //
    // Function Declarations
    //

    // Initialization
    bool Core_Initialize(void);
    void Core_Shutdown(void);
    bool Core_EnablePrivileges(void);
    bool Core_IsElevated(void);

    // Data Collection (threaded, non-blocking)
    void Core_RefreshMemoryStats(MEMORY_STATS* stats);
    void Core_RefreshProcessList(PROCESS_LIST* list);
    void Core_RefreshPhysicalMap(PHYSICAL_MEMORY_MAP* map);
    void Core_RefreshPrefetchList(PREFETCH_LIST* list);

    // Extended NT Functions
    void Core_RefreshPerformanceStats(PERF_STATS* stats);
    void Core_RefreshPoolTags(POOLTAG_LIST* list);
    void Core_RefreshHandleStats(HANDLE_STATS* stats);
    void Core_RefreshHandleDetails(HANDLE_DETAIL_LIST* list, ULONG filterPid, USHORT filterType);
    void Core_FreeHandleDetails(HANDLE_DETAIL_LIST* list);
    void Core_RefreshCompressionStats(COMPRESSION_STATS* stats);

    // Superfetch Advanced Functions
    bool Core_QuerySuperfetchInfo(SUPERFETCH_INFORMATION_CLASS infoClass, PVOID buffer, ULONG length, PULONG returnLength);
    bool Core_RefreshSuperfetchProcesses(PF_PRIVSOURCE_INFO** procs, uint32_t* count);
    bool Core_QueryPfnDatabase(uint64_t startPfn, uint64_t pfnCount, PFN_QUERY_RESULTS* results);
    bool Core_TranslateVirtualAddress(PVOID virtualAddr, uint64_t* physicalAddr, PFN_INFO* info);
    bool Core_QueryMemoryRangesV2(PHYSICAL_MEMORY_MAP* map);
    NTSTATUS Core_GetLastSuperfetchError(void);
    size_t Core_GetSuperfetchInfoSize(void);

    // Utilities
    const char* Core_FormatBytes(uint64_t bytes, char* buffer, size_t bufferSize);
    const char* Core_FormatPages(uint64_t pages, char* buffer, size_t bufferSize);
    const char* Core_FormatNumber(uint64_t num, char* buffer, size_t bufferSize);
    uint64_t Core_GetTickCount64(void);

    // Kernel Object Functions (EPROCESS, KTHREAD, etc.)
    uint64_t Core_GetKernelObject(ULONG processId, HANDLE handle);
    uint64_t Core_GetEProcess(ULONG processId);
    uint64_t Core_GetCurrentEProcess(void);
    uint64_t Core_GetKThread(ULONG processId, ULONG threadId);
    uint64_t Core_GetCurrentKThread(void);

    // Kernel Object Info structure - comprehensive process/thread info
    typedef struct _KERNEL_OBJECT_INFO {
        uint64_t EProcess;
        uint64_t Peb;
        uint64_t KThread;
        uint64_t Teb;
        uint64_t DirectoryTableBase;  // CR3 value
        uint64_t Token;               // Process token address
        uint64_t ObjectTable;         // Handle table
        uint64_t VadRoot;             // VAD tree root
        uint64_t UniqueProcessKey;    // Unique key
        uint64_t SecurityPort;        // LPC security port
        uint64_t InheritedFromPid;    // Inherited from
        ULONG ProcessId;
        ULONG ParentPid;
        ULONG SessionId;
        ULONG HandleCount;
        ULONG ThreadCount;
        ULONG64 CreateTime;
        ULONG64 UserTime;
        ULONG64 KernelTime;
        wchar_t ImageName[64];
        bool Valid;
        bool IsWow64;                 // 32-bit process on 64-bit OS
        bool IsProtected;             // Protected process
    } KERNEL_OBJECT_INFO;

    bool Core_GetKernelProcessInfo(ULONG processId, KERNEL_OBJECT_INFO* info);

    // Kernel Driver Functions
    bool Core_RefreshKernelDrivers(KERNEL_DRIVER_LIST* list);
    bool Core_GetKernelImageInfo(uint64_t* imageBase, uint32_t* imageSize, char* name, size_t nameSize);
    PVOID Core_GetSystemInformation(SYSTEM_INFORMATION_CLASS infoClass);

    // PhysicalMemory Object Functions (for DKOM)
    bool Core_FindPhysicalMemoryObject(PHYSICALMEMORY_OBJECT_INFO* info);
    
    // Ntoskrnl Export Functions
    bool Core_RefreshKernelExports(KERNEL_EXPORT_LIST* list, KERNEL_DRIVER_LIST* drivers, uint32_t driverIndex);
    uint64_t Core_GetKernelProcAddress(KERNEL_EXPORT_LIST* exports, const char* procName);
    bool Core_GetSyscallNumber(const char* procName, uint32_t* syscallNumber);
    uint32_t Core_LdrGetProcAddress(PVOID image, const char* name);
    bool Core_MapKernelImage(const char* systemPath, PVOID* mappedImage, uint32_t* imageSize);
    void Core_FreeKernelExports(KERNEL_EXPORT_LIST* list);
    bool Core_FindKernelPointers(KERNEL_POINTERS* ptrs, KERNEL_EXPORT_LIST* exports, KERNEL_DRIVER_LIST* drivers);

    //
    // Inline Helpers
    //

    static inline void SUPERFETCH_INFO_INIT(
        PSUPERFETCH_INFORMATION info,
        SUPERFETCH_INFORMATION_CLASS infoClass,
        PVOID data,
        ULONG length
    ) {
        info->Version = SUPERFETCH_VERSION;
        info->Magic = SUPERFETCH_MAGIC;
        info->InfoClass = infoClass;
        info->Data = data;
        info->Length = length;
    }

    static inline uint64_t PagesToMB(uint64_t pages) {
        return (pages * PAGE_SIZE) / (1024 * 1024);
    }

    static inline uint64_t PagesToGB(uint64_t pages) {
        return (pages * PAGE_SIZE) / (1024 * 1024 * 1024);
    }

    static inline double PagesToGBf(uint64_t pages) {
        return (double)(pages * PAGE_SIZE) / (1024.0 * 1024.0 * 1024.0);
    }

#ifdef __cplusplus
}
#endif
