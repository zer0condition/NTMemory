#define _CRT_SECURE_NO_WARNINGS
#include "core.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"
ImFont* descriptionfont = nullptr;
ImFont* icons = nullptr;
#include <d3d11.h>
#include <tchar.h>
#include <windowsx.h> 

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

static APP_STATE g_State = {};
static PHYSICALMEMORY_OBJECT_INFO g_PhysMemInfo = {};
static bool g_PhysMemQueried = false;
static bool g_Running = true;
static HWND g_hWnd = nullptr;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Window drag state
static bool g_Dragging = false;
static POINT g_DragStart = {0, 0};
static bool g_Maximized = false;

static bool CopyableCell(const char* label, const char* copyValue = nullptr) {
    const char* textToCopy = copyValue ? copyValue : label;
    
    ImGui::Text("%s", label);
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    
    if (ImGui::IsItemClicked()) {
        ImGui::SetClipboardText(textToCopy);
        return true;
    }
    return false;
}

// Helper: Copyable text with color for table cells
static bool CopyableCellColored(const ImVec4& col, const char* label, const char* copyValue = nullptr) {
    const char* textToCopy = copyValue ? copyValue : label;
    
    ImGui::TextColored(col, "%s", label);
    
    if (ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    
    if (ImGui::IsItemClicked()) {
        ImGui::SetClipboardText(textToCopy);
        return true;
    }
    return false;
}

static void SetupImGuiStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;
    
    // Clean dark theme - code editor inspired
    colors[ImGuiCol_WindowBg]           = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_ChildBg]            = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]            = ImVec4(0.14f, 0.14f, 0.16f, 0.98f);
    colors[ImGuiCol_Border]             = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_BorderShadow]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    
    // Text - clean white
    colors[ImGuiCol_Text]               = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]       = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    
    // Frames
    colors[ImGuiCol_FrameBg]            = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]     = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    colors[ImGuiCol_FrameBgActive]      = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    
    // Title bar
    colors[ImGuiCol_TitleBg]            = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive]      = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]   = ImVec4(0.10f, 0.10f, 0.12f, 0.75f);
    colors[ImGuiCol_MenuBarBg]          = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    
    // Scrollbar - thin and subtle
    colors[ImGuiCol_ScrollbarBg]        = ImVec4(0.10f, 0.10f, 0.12f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]      = ImVec4(0.30f, 0.30f, 0.34f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]= ImVec4(0.50f, 0.50f, 0.54f, 1.00f);
    
    // Accent - subtle blue
    colors[ImGuiCol_CheckMark]          = ImVec4(0.45f, 0.65f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrab]         = ImVec4(0.40f, 0.55f, 0.75f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]   = ImVec4(0.50f, 0.65f, 0.85f, 1.00f);
    
    // Buttons - subtle
    colors[ImGuiCol_Button]             = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_ButtonHovered]      = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_ButtonActive]       = ImVec4(0.35f, 0.45f, 0.58f, 1.00f);
    
    // Headers
    colors[ImGuiCol_Header]             = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_HeaderHovered]      = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    colors[ImGuiCol_HeaderActive]       = ImVec4(0.32f, 0.42f, 0.55f, 1.00f);
    
    // Separators
    colors[ImGuiCol_Separator]          = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]   = ImVec4(0.35f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_SeparatorActive]    = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
    
    // Resize grip
    colors[ImGuiCol_ResizeGrip]         = ImVec4(0.30f, 0.30f, 0.34f, 0.50f);
    colors[ImGuiCol_ResizeGripHovered]  = ImVec4(0.40f, 0.55f, 0.70f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]   = ImVec4(0.50f, 0.65f, 0.80f, 0.95f);
    
    // Tabs
    colors[ImGuiCol_Tab]                = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered]         = ImVec4(0.28f, 0.38f, 0.50f, 1.00f);
    colors[ImGuiCol_TabActive]          = ImVec4(0.22f, 0.32f, 0.45f, 1.00f);
    colors[ImGuiCol_TabUnfocused]       = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.25f, 0.35f, 1.00f);
    
    // Plots
    colors[ImGuiCol_PlotLines]          = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]   = ImVec4(0.60f, 0.80f, 1.00f, 1.00f);
    colors[ImGuiCol_PlotHistogram]      = ImVec4(0.40f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.50f, 0.70f, 0.90f, 1.00f);
    
    // Tables
    colors[ImGuiCol_TableHeaderBg]      = ImVec4(0.16f, 0.16f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]  = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    colors[ImGuiCol_TableBorderLight]   = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    colors[ImGuiCol_TableRowBg]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]      = ImVec4(0.14f, 0.14f, 0.16f, 0.50f);
    
    // Selection
    colors[ImGuiCol_TextSelectedBg]     = ImVec4(0.30f, 0.45f, 0.65f, 0.45f);
    colors[ImGuiCol_DragDropTarget]     = ImVec4(0.50f, 0.70f, 0.90f, 0.90f);
    colors[ImGuiCol_NavHighlight]       = ImVec4(0.45f, 0.65f, 0.85f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]  = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]   = ImVec4(0.10f, 0.10f, 0.12f, 0.65f);
    
    // Minimal rounding
    style.WindowRounding = 0.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;
    
    // Compact spacing
    style.WindowPadding = ImVec2(8, 8);
    style.FramePadding = ImVec2(6, 4);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(6, 4);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;
    
    // Thin borders
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;
    
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
}

//
// Memory Overview Panel (Enhanced)
//

static void DrawMemoryOverviewInner() {
    char buf[64];
    char buf2[64];
    MEMORY_STATS* stats = &g_State.MemStats;
    
    ImGui::BeginChild("MemOverview", ImVec2(-1, -1), true);
    ImGui::Text("MEMORY OVERVIEW");
    ImGui::Separator();
    
    // Progress bars
    float totalMem = (float)stats->TotalPhysical;
    float usedMem = totalMem - (float)stats->AvailablePhysical;
    float usedPercent = totalMem > 0 ? usedMem / totalMem : 0;
    
    ImGui::Text("Physical Memory");
    ImGui::ProgressBar(usedPercent, ImVec2(-1, 16));
    ImGui::Text("%s / %s (%.1f%% used)", 
        Core_FormatBytes((uint64_t)usedMem, buf, sizeof(buf)),
        Core_FormatBytes(stats->TotalPhysical, buf2, sizeof(buf2)),
        usedPercent * 100.0f);
    
    // Pagefile
    float pfUsedPercent = stats->PagefileTotal > 0 ? 
        (float)stats->PagefileInUse / stats->PagefileTotal : 0;
    ImGui::Text("Page File");
    ImGui::ProgressBar(pfUsedPercent, ImVec2(-1, 16));
    ImGui::Text("%s / %s", 
        Core_FormatBytes(stats->PagefileInUse, buf, sizeof(buf)),
        Core_FormatBytes(stats->PagefileTotal, buf2, sizeof(buf2)));
    
    // Commit charge
    float commitPercent = stats->CommitLimit > 0 ? 
        (float)stats->CommittedPages / stats->CommitLimit : 0;
    ImGui::Text("Commit Charge");
    ImGui::ProgressBar(commitPercent, ImVec2(-1, 16));
    ImGui::Text("%s / %s (Peak: %s)", 
        Core_FormatPages(stats->CommittedPages, buf, sizeof(buf)),
        Core_FormatPages(stats->CommitLimit, buf2, sizeof(buf2)),
        Core_FormatPages(stats->PeakCommitment, buf + 32, sizeof(buf) - 32));
    
    // File Cache
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "File Cache: %s (Peak: %s)", 
        Core_FormatBytes(stats->FileCacheSize, buf, sizeof(buf)),
        Core_FormatBytes(stats->FileCachePeak, buf2, sizeof(buf2)));
    
    ImGui::EndChild();
}

static void DrawMemoryOverview() {
    char buf[64];
    char buf2[64];
    MEMORY_STATS* stats = &g_State.MemStats;
    
    ImGui::BeginChild("MemOverview", ImVec2(0, 220), true);
    ImGui::Text("MEMORY OVERVIEW");
    ImGui::Separator();
    
    // Progress bars
    float totalMem = (float)stats->TotalPhysical;
    float usedMem = totalMem - (float)stats->AvailablePhysical;
    float usedPercent = totalMem > 0 ? usedMem / totalMem : 0;
    
    ImGui::Text("Physical Memory");
    ImGui::ProgressBar(usedPercent, ImVec2(-1, 18));
    ImGui::Text("%s / %s (%.1f%% used)", 
        Core_FormatBytes((uint64_t)usedMem, buf, sizeof(buf)),
        Core_FormatBytes(stats->TotalPhysical, buf2, sizeof(buf2)),
        usedPercent * 100.0f);
    
    ImGui::Spacing();
    
    // Pagefile
    float pfUsedPercent = stats->PagefileTotal > 0 ? 
        (float)stats->PagefileInUse / stats->PagefileTotal : 0;
    ImGui::Text("Page File");
    ImGui::ProgressBar(pfUsedPercent, ImVec2(-1, 18));
    ImGui::Text("%s / %s", 
        Core_FormatBytes(stats->PagefileInUse, buf, sizeof(buf)),
        Core_FormatBytes(stats->PagefileTotal, buf2, sizeof(buf2)));
    
    ImGui::Spacing();
    
    // Commit charge
    float commitPercent = stats->CommitLimit > 0 ? 
        (float)stats->CommittedPages / stats->CommitLimit : 0;
    ImGui::Text("Commit Charge");
    ImGui::ProgressBar(commitPercent, ImVec2(-1, 18));
    ImGui::Text("%s / %s (Peak: %s)", 
        Core_FormatPages(stats->CommittedPages, buf, sizeof(buf)),
        Core_FormatPages(stats->CommitLimit, buf2, sizeof(buf2)),
        Core_FormatPages(stats->PeakCommitment, buf + 32, sizeof(buf) - 32));
    
    ImGui::Spacing();
    
    // File Cache
    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "File Cache: %s (Peak: %s)", 
        Core_FormatBytes(stats->FileCacheSize, buf, sizeof(buf)),
        Core_FormatBytes(stats->FileCachePeak, buf2, sizeof(buf2)));
    
    ImGui::EndChild();
}

//
// Standby List Panel
//

static void DrawStandbyPanelInner() {
    MEMORY_STATS* stats = &g_State.MemStats;
    char buf[64];
    
    ImGui::BeginChild("StandbyPanel", ImVec2(-1, -1), true);
    ImGui::Text("STANDBY LIST PRIORITIES");
    ImGui::Separator();
    
    float maxCount = 1.0f;
    for (int i = 0; i < 8; i++) {
        if (stats->StandbyPageCount[i] > maxCount) {
            maxCount = (float)stats->StandbyPageCount[i];
        }
    }
    
    ImVec4 barColors[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.4f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.7f, 0.9f, 0.3f, 1.0f),
        ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
        ImVec4(0.3f, 0.8f, 0.6f, 1.0f),
        ImVec4(0.3f, 0.7f, 0.9f, 1.0f),
    };
    
    for (int i = 0; i < 8; i++) {
        float fraction = stats->StandbyPageCount[i] / maxCount;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColors[i]);
        ImGui::ProgressBar(fraction, ImVec2(150, 12), "");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("P%d: %s", i, Core_FormatPages(stats->StandbyPageCount[i], buf, sizeof(buf)));
    }
    
    ImGui::Text("Total Standby: %s", Core_FormatPages(stats->TotalStandby, buf, sizeof(buf)));
    ImGui::EndChild();
}

static void DrawStandbyPanel() {
    MEMORY_STATS* stats = &g_State.MemStats;
    char buf[64];
    
    ImGui::BeginChild("StandbyPanel", ImVec2(0, 220), true);
    ImGui::Text("STANDBY LIST PRIORITIES");
    ImGui::Separator();
    
    // Bar chart visualization
    float maxCount = 1.0f;
    for (int i = 0; i < 8; i++) {
        if (stats->StandbyPageCount[i] > maxCount) {
            maxCount = (float)stats->StandbyPageCount[i];
        }
    }
    
    ImVec4 barColors[] = {
        ImVec4(0.8f, 0.2f, 0.2f, 1.0f),  // Red (low priority)
        ImVec4(0.9f, 0.4f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.6f, 0.2f, 1.0f),
        ImVec4(0.9f, 0.8f, 0.2f, 1.0f),
        ImVec4(0.7f, 0.9f, 0.3f, 1.0f),
        ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
        ImVec4(0.3f, 0.8f, 0.6f, 1.0f),
        ImVec4(0.3f, 0.7f, 0.9f, 1.0f),  // Blue (high priority)
    };
    
    for (int i = 0; i < 8; i++) {
        float fraction = stats->StandbyPageCount[i] / maxCount;
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, barColors[i]);
        ImGui::ProgressBar(fraction, ImVec2(180, 14), "");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::Text("P%d: %s", i, Core_FormatPages(stats->StandbyPageCount[i], buf, sizeof(buf)));
    }
    
    ImGui::Spacing();
    ImGui::Text("Total Standby: %s", Core_FormatPages(stats->TotalStandby, buf, sizeof(buf)));
    
    ImGui::EndChild();
}

//
// Memory Lists Panel (Enhanced)
//

static void DrawMemoryListsInner() {
    MEMORY_STATS* stats = &g_State.MemStats;
    char buf[64];
    
    ImGui::BeginChild("MemLists", ImVec2(-1, -1), true);
    ImGui::Text("MEMORY LISTS & POOLS");
    ImGui::Separator();
    
    ImGui::Columns(2, "memcols", false);
    
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Free:");
    ImGui::Text("  Zeroed: %s", Core_FormatPages(stats->ZeroPageCount, buf, sizeof(buf)));
    ImGui::Text("  Free:   %s", Core_FormatPages(stats->FreePageCount, buf, sizeof(buf)));
    
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Pools:");
    ImGui::Text("  Paged:    %s", Core_FormatPages(stats->PagedPoolPages, buf, sizeof(buf)));
    ImGui::Text("  NonPaged: %s", Core_FormatPages(stats->NonPagedPoolPages, buf, sizeof(buf)));
    
    ImGui::NextColumn();
    
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f), "Modified:");
    ImGui::Text("  Modified:   %s", Core_FormatPages(stats->ModifiedPageCount, buf, sizeof(buf)));
    ImGui::Text("  No-Write:   %s", Core_FormatPages(stats->ModifiedNoWritePageCount, buf, sizeof(buf)));
    
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Counters:");
    ImGui::Text("  CtxSwitch: %s", Core_FormatNumber(stats->ContextSwitches, buf, sizeof(buf)));
    ImGui::Text("  SysCalls:  %s", Core_FormatNumber(stats->SystemCalls, buf, sizeof(buf)));
    
    ImGui::Columns(1);
    ImGui::EndChild();
}

static void DrawMemoryLists() {
    MEMORY_STATS* stats = &g_State.MemStats;
    char buf[64];
    char buf2[64];
    
    ImGui::BeginChild("MemLists", ImVec2(0, 160), true);
    ImGui::Text("MEMORY LISTS & POOLS");
    ImGui::Separator();
    
    ImGui::Columns(2, "memcols", false);
    
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Free:");
    ImGui::Text("  Zeroed: %s", Core_FormatPages(stats->ZeroPageCount, buf, sizeof(buf)));
    ImGui::Text("  Free:   %s", Core_FormatPages(stats->FreePageCount, buf, sizeof(buf)));
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "Pools:");
    ImGui::Text("  Paged:    %s", Core_FormatPages(stats->PagedPoolPages, buf, sizeof(buf)));
    ImGui::Text("  NonPaged: %s", Core_FormatPages(stats->NonPagedPoolPages, buf, sizeof(buf)));
    
    ImGui::NextColumn();
    
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.3f, 1.0f), "Modified:");
    ImGui::Text("  Modified:   %s", Core_FormatPages(stats->ModifiedPageCount, buf, sizeof(buf)));
    ImGui::Text("  No-Write:   %s", Core_FormatPages(stats->ModifiedNoWritePageCount, buf, sizeof(buf)));
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Counters:");
    ImGui::Text("  CtxSwitch: %s", Core_FormatNumber(stats->ContextSwitches, buf, sizeof(buf)));
    ImGui::Text("  SysCalls:  %s", Core_FormatNumber(stats->SystemCalls, buf, sizeof(buf)));
    
    ImGui::Columns(1);
    
    if (stats->BadPageCount > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Bad Pages: %llu", stats->BadPageCount);
    }
    
    ImGui::EndChild();
}

//
// Real-time Graph
//

static void DrawHistoryGraphInner() {
    ImGui::BeginChild("GraphPanel", ImVec2(-1, -1), true);
    ImGui::Text("MEMORY HISTORY");
    ImGui::Separator();
    
    float h = (ImGui::GetContentRegionAvail().y - 8) / 2;
    ImGui::PlotLines("##standby", g_State.StandbyHistory, 120, g_State.HistoryIndex,
        "Standby", 0.0f, 100.0f, ImVec2(-1, h));
    ImGui::PlotLines("##free", g_State.FreeHistory, 120, g_State.HistoryIndex,
        "Free", 0.0f, 100.0f, ImVec2(-1, h));
    
    ImGui::EndChild();
}

static void DrawHistoryGraph() {
    ImGui::BeginChild("GraphPanel", ImVec2(0, 130), true);
    ImGui::Text("MEMORY HISTORY");
    ImGui::Separator();
    
    // Draw stacked area chart simulation with plot lines
    ImGui::PlotLines("##standby", g_State.StandbyHistory, 120, g_State.HistoryIndex,
        "Standby", 0.0f, 100.0f, ImVec2(-1, 50));
    ImGui::PlotLines("##free", g_State.FreeHistory, 120, g_State.HistoryIndex,
        "Free", 0.0f, 100.0f, ImVec2(-1, 50));
    
    ImGui::EndChild();
}

//
// Compression Stats Panel (Win10+)
//

static void DrawCompressionPanelInner() {
    COMPRESSION_STATS* stats = &g_State.CompressionStats;
    char buf[64];
    char buf2[64];
    
    ImGui::BeginChild("CompressionPanel", ImVec2(-1, -1), true);
    ImGui::Text("MEMORY COMPRESSION (Win10+)");
    ImGui::Separator();
    
    if (stats->Available) {
        ImGui::Text("Compression Process PID: %llu", stats->CompressionPid);
        ImGui::Text("Store Working Set: %s", Core_FormatBytes(stats->WorkingSetSize, buf, sizeof(buf)));
        ImGui::Text("Data Compressed: %s -> %s", 
            Core_FormatBytes(stats->TotalDataCompressed, buf, sizeof(buf)),
            Core_FormatBytes(stats->TotalCompressedSize, buf2, sizeof(buf2)));
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Compression stats not available");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Requires Windows 10+)");
    }
    
    ImGui::EndChild();
}

static void DrawCompressionPanel() {
    COMPRESSION_STATS* stats = &g_State.CompressionStats;
    char buf[64];
    char buf2[64];
    
    ImGui::BeginChild("CompressionPanel", ImVec2(0, 110), true);
    ImGui::Text("MEMORY COMPRESSION (Win10+)");
    ImGui::Separator();
    
    if (stats->Available) {
        ImGui::Text("Compression Process PID: %llu", stats->CompressionPid);
        ImGui::Text("Store Working Set: %s", Core_FormatBytes(stats->WorkingSetSize, buf, sizeof(buf)));
        ImGui::Text("Data Compressed: %s -> %s", 
            Core_FormatBytes(stats->TotalDataCompressed, buf, sizeof(buf)),
            Core_FormatBytes(stats->TotalCompressedSize, buf2, sizeof(buf2)));
        ImGui::Text("Savings: %s (Ratio: %.2fx)", 
            Core_FormatBytes(stats->CompressionSavings, buf, sizeof(buf)),
            stats->CompressionRatio);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Compression stats not available");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(Requires Windows 10+)");
    }
    
    ImGui::EndChild();
}

//
// System Summary Panel (for Overview tab)
//

static void DrawSystemSummaryInner() {
    KERNEL_DRIVER_LIST* drivers = &g_State.DriverList;
    KERNEL_EXPORT_LIST* exports = &g_State.ExportList;
    KERNEL_POINTERS* kptrs = &g_State.KernelPtrs;
    char buf[64];
    
    ImGui::BeginChild("SystemSummary", ImVec2(-1, -1), true);
    ImGui::Text("KERNEL INFORMATION");
    ImGui::Separator();
    
    if (drivers->Drivers == NULL || drivers->Count == 0) {
        Core_RefreshKernelDrivers(drivers);
    }
    
    if (!kptrs->Valid) {
        if (exports->MappedImage == NULL && drivers->NtoskrnlBase != 0) {
            Core_RefreshKernelExports(exports, drivers, 0);
        }
        if (exports->MappedImage != NULL) {
            Core_FindKernelPointers(kptrs, exports, drivers);
        }
    }
    
    if (drivers->NtoskrnlBase != 0) {
        ImGui::Text("Kernel: %s", drivers->NtoskrnlName);
        char baseStr[24];
        snprintf(baseStr, sizeof(baseStr), "0x%016llX", drivers->NtoskrnlBase);
        ImGui::Text("Base: ");
        ImGui::SameLine(0, 0);
        CopyableCellColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), baseStr);
        ImGui::Text("Size: %s  |  Drivers: %u", 
            Core_FormatBytes(drivers->NtoskrnlSize, buf, sizeof(buf)), drivers->Count);
    }
    
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "Kernel Pointers:");
    
    if (kptrs->Valid) {
        if (kptrs->KeServiceDescriptorTable) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->KeServiceDescriptorTable);
            ImGui::Text("SSDT: "); ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), buf);
        }
        if (kptrs->KiSystemCall64) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->KiSystemCall64);
            ImGui::Text("KiSystemCall64: "); ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), buf);
        }
        if (kptrs->HalDispatchTable) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->HalDispatchTable);
            ImGui::Text("HalDispatch: "); ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), buf);
        }
        if (kptrs->PsLoadedModuleList) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->PsLoadedModuleList);
            ImGui::Text("PsLoadedModuleList: "); ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(1.0f, 0.7f, 0.5f, 1.0f), buf);
        }
        
        // PhysicalMemory Object Header - always query and show result
        if (!g_PhysMemQueried) {
            g_PhysMemQueried = true;
            Core_FindPhysicalMemoryObject(&g_PhysMemInfo);
        }
        if (g_PhysMemInfo.Found) {
            snprintf(buf, sizeof(buf), "0x%016llX", g_PhysMemInfo.ObjectHeaderAddress);
            ImGui::Text("\\Device\\PhysicalMemory: "); ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), buf);
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "\\Device\\PhysicalMemory: Not found");
        }
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Loading...");
    }
    
    ImGui::EndChild();
}

static void DrawSystemSummary() {
    KERNEL_DRIVER_LIST* drivers = &g_State.DriverList;
    KERNEL_EXPORT_LIST* exports = &g_State.ExportList;
    KERNEL_POINTERS* kptrs = &g_State.KernelPtrs;
    char buf[64];
    
    ImGui::BeginChild("SystemSummary", ImVec2(0, 180), true);
    ImGui::Text("KERNEL INFORMATION");
    ImGui::Separator();
    
    // Ensure drivers are loaded for kernel info
    if (drivers->Drivers == NULL || drivers->Count == 0) {
        Core_RefreshKernelDrivers(drivers);
    }
    
    // Auto-load exports and find kernel pointers
    if (!kptrs->Valid) {
        if (exports->MappedImage == NULL && drivers->NtoskrnlBase != 0) {
            Core_RefreshKernelExports(exports, drivers, 0);
        }
        if (exports->MappedImage != NULL) {
            Core_FindKernelPointers(kptrs, exports, drivers);
        }
    }
    
    // Kernel info
    if (drivers->NtoskrnlBase != 0) {
        ImGui::Text("Kernel: %s", drivers->NtoskrnlName);
        
        char baseStr[24];
        snprintf(baseStr, sizeof(baseStr), "0x%016llX", drivers->NtoskrnlBase);
        ImGui::Text("Base: ");
        ImGui::SameLine(0, 0);
        CopyableCellColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), baseStr);
        
        ImGui::Text("Size: %s  |  Drivers: %u", 
            Core_FormatBytes(drivers->NtoskrnlSize, buf, sizeof(buf)), 
            drivers->Count);
    }
    
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.5f, 1.0f), "Kernel Pointers:");
    
    // Show important kernel pointers
    if (kptrs->Valid) {
        if (kptrs->KeServiceDescriptorTable) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->KeServiceDescriptorTable);
            ImGui::Text("SSDT: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), buf);
        }
        if (kptrs->KiSystemCall64) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->KiSystemCall64);
            ImGui::Text("KiSystemCall64: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), buf);
        }
        if (kptrs->HalDispatchTable) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->HalDispatchTable);
            ImGui::Text("HalDispatch: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), buf);
        }
        if (kptrs->PsLoadedModuleList) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->PsLoadedModuleList);
            ImGui::Text("PsLoadedModuleList: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(1.0f, 0.7f, 0.5f, 1.0f), buf);
        }
        if (kptrs->PsActiveProcessHead) {
            snprintf(buf, sizeof(buf), "0x%016llX", kptrs->PsActiveProcessHead);
            ImGui::Text("PsActiveProcessHead: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(1.0f, 0.5f, 0.7f, 1.0f), buf);
        }
        
        // PhysicalMemory Object Header
        if (!g_PhysMemQueried) {
            g_PhysMemQueried = true;
            Core_FindPhysicalMemoryObject(&g_PhysMemInfo);
        }
        if (g_PhysMemInfo.Found) {
            snprintf(buf, sizeof(buf), "0x%016llX", g_PhysMemInfo.ObjectHeaderAddress);
            ImGui::Text("PhysMem ObjHeader: ");
            ImGui::SameLine(0, 0);
            CopyableCellColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), buf);
        }
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Load exports to find pointers");
    }
    
    ImGui::EndChild();
}

//
// Pool Tags Panel
//

static void DrawPoolTagsPanel() {
    POOLTAG_LIST* list = &g_State.PoolTags;
    char buf[64];
    
    ImGui::BeginChild("PoolTagsPanel", ImVec2(0, 0), true);
    ImGui::Text("KERNEL POOL TAGS");
    ImGui::Separator();
    
    ImGui::Text("Total Paged: %s | NonPaged: %s | Tags: %u",
        Core_FormatBytes(list->TotalPagedUsed, buf, sizeof(buf)),
        Core_FormatBytes(list->TotalNonPagedUsed, buf + 32, sizeof(buf) - 32),
        list->Count);
    
    ImGui::Spacing();
    
    if (ImGui::BeginTable("pooltags", 5,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        
        ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Paged Used", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("NonPaged Used", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Allocs", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        for (uint32_t i = 0; i < list->Count && i < 500; i++) {
            POOLTAG_INFO* tag = &list->Tags[i];
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            CopyableCell(tag->Tag);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", Core_FormatBytes(tag->PagedUsed, buf, sizeof(buf)));
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", Core_FormatBytes(tag->NonPagedUsed, buf, sizeof(buf)));
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%u/%u", tag->PagedAllocs, tag->NonPagedAllocs);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", Core_FormatBytes(tag->TotalUsed, buf, sizeof(buf)));
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

//
// Handle Stats Panel
//

// Static state for handle details view
static HANDLE_DETAIL_LIST g_HandleDetails = {0};
static int g_SelectedHandleProcIdx = -1;
static bool g_ShowHandleDetails = false;

static void DrawHandleStatsPanel() {
    HANDLE_STATS* stats = &g_State.HandleStats;
    char buf[64];
    
    ImGui::BeginChild("HandleStatsPanel", ImVec2(0, 0), true);
    
    // Header with refresh button - right aligned
    if (g_ShowHandleDetails) {
        if (ImGui::Button("< Back")) {
            g_ShowHandleDetails = false;
            g_SelectedHandleProcIdx = -1;
        }
        ImGui::SameLine();
    }
    ImGui::Text("SYSTEM HANDLE INFORMATION");
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70);
    if (ImGui::Button("Refresh")) {
        Core_RefreshHandleStats(stats);
        g_SelectedHandleProcIdx = -1;
        g_ShowHandleDetails = false;
        Core_FreeHandleDetails(&g_HandleDetails);
    }
    ImGui::Separator();
    
    // Summary stats
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Total:");
    ImGui::SameLine();
    ImGui::Text("%s handles", Core_FormatNumber(stats->TotalHandles, buf, sizeof(buf)));
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "| Processes:");
    ImGui::SameLine();
    ImGui::Text("%u", stats->ProcessCount);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "| Types:");
    ImGui::SameLine();
    ImGui::Text("%u", stats->TypeCount);
    
    ImGui::Spacing();
    
    if (g_ShowHandleDetails && g_SelectedHandleProcIdx >= 0 && (uint32_t)g_SelectedHandleProcIdx < stats->ProcessCount) {
        // Detailed view for selected process
        HANDLE_PROCESS_INFO* proc = &stats->Processes[g_SelectedHandleProcIdx];
        char narrowName[64];
        wcstombs(narrowName, proc->ProcessName, sizeof(narrowName));
        
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Handle Details for: %s (PID %u)", 
            narrowName, proc->ProcessId);
        ImGui::Separator();
        
        // Type breakdown for this process - simple colored text
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "File:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->FileHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Key:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->KeyHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Event:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->EventHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "Mutant:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->MutantHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Section:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->SectionHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Thread:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->ThreadHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Process:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->ProcessHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Token:");
        ImGui::SameLine(0, 2);
        ImGui::Text("%-6u", proc->TokenHandles);
        ImGui::SameLine(0, 10);
        
        ImGui::TextDisabled("Other:");
        ImGui::SameLine(0, 2);
        ImGui::TextDisabled("%u", proc->OtherHandles);
        
        ImGui::Spacing();
        
        // Handle list table
        if (ImGui::BeginTable("handledetails", 5,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | 
            ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
            
            ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Object Address", ImGuiTableColumnFlags_WidthFixed, 150);
            ImGui::TableSetupColumn("Access", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            
            for (uint32_t i = 0; i < g_HandleDetails.Count; i++) {
                HANDLE_DETAIL* det = &g_HandleDetails.Handles[i];
                
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                char handleStr[16];
                snprintf(handleStr, sizeof(handleStr), "0x%04llX", det->HandleValue);
                CopyableCell(handleStr);
                
                ImGui::TableSetColumnIndex(1);
                // Color by type
                if (strcmp(det->TypeName, "File") == 0 || strcmp(det->TypeName, "Key") == 0) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", det->TypeName);
                } else if (strcmp(det->TypeName, "Process") == 0 || strcmp(det->TypeName, "Thread") == 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "%s", det->TypeName);
                } else if (strcmp(det->TypeName, "Event") == 0 || strcmp(det->TypeName, "Mutant") == 0) {
                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", det->TypeName);
                } else if (strcmp(det->TypeName, "Section") == 0 || strcmp(det->TypeName, "Token") == 0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", det->TypeName);
                } else {
                    ImGui::Text("%s", det->TypeName);
                }
                
                ImGui::TableSetColumnIndex(2);
                char objAddr[24];
                snprintf(objAddr, sizeof(objAddr), "0x%016llX", det->ObjectAddress);
                CopyableCellColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), objAddr);
                
                ImGui::TableSetColumnIndex(3);
                char accessStr[16];
                snprintf(accessStr, sizeof(accessStr), "0x%08X", det->GrantedAccess);
                ImGui::TextDisabled("%s", accessStr);
                
                ImGui::TableSetColumnIndex(4);
                // access rights
                bool first = true;
                if (det->GrantedAccess & 0x001F0000) {
                    ImGui::TextDisabled("STANDARD");
                    first = false;
                }
                if (det->GrantedAccess & 0x0001) {
                    if (!first) ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 0.5f, 1.0f), "READ");
                    first = false;
                }
                if (det->GrantedAccess & 0x0002) {
                    if (!first) ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "WRITE");
                    first = false;
                }
                if (det->GrantedAccess & 0x10000000) {
                    if (!first) ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "GENERIC_ALL");
                    first = false;
                }
                if (first && det->GrantedAccess != 0) {
                    ImGui::TextDisabled("-");
                }
            }
            
            ImGui::EndTable();
        }
        
        if (g_HandleDetails.Count >= 5000) {
            ImGui::TextDisabled("(Showing first 5000 handles)");
        }
        
    } else {
        // Overview mode - two columns
        float totalWidth = ImGui::GetContentRegionAvail().x;
        float columnWidth = (totalWidth - 20) / 2;
        
        // Left column: Object Types
        ImGui::BeginChild("HandleTypesCol", ImVec2(columnWidth, 0), true);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Handle Types by Count");
        ImGui::Separator();
        
        if (ImGui::BeginTable("handletypes", 3,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
            
            ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 35);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Count", ImGuiTableColumnFlags_WidthFixed, 80);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            
            // Sort types by count
            typedef struct { int index; uint64_t count; } TypeEntry;
            TypeEntry types[64];
            int typeCount = 0;
            
            for (int i = 0; i < 64; i++) {
                if (stats->HandlesByType[i] > 0) {
                    types[typeCount].index = i;
                    types[typeCount].count = stats->HandlesByType[i];
                    typeCount++;
                }
            }
            
            // Simple bubble sort for display (small array)
            for (int i = 0; i < typeCount - 1; i++) {
                for (int j = 0; j < typeCount - i - 1; j++) {
                    if (types[j].count < types[j + 1].count) {
                        TypeEntry temp = types[j];
                        types[j] = types[j + 1];
                        types[j + 1] = temp;
                    }
                }
            }
            
            for (int i = 0; i < typeCount; i++) {
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%d", types[i].index);
                
                ImGui::TableSetColumnIndex(1);
                const char* typeName = stats->TypeNames[types[i].index];
                if (typeName[0] != '\0') {
                    // Color important types
                    if (strcmp(typeName, "File") == 0 || strcmp(typeName, "Key") == 0) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", typeName);
                    } else if (strcmp(typeName, "Process") == 0 || strcmp(typeName, "Thread") == 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "%s", typeName);
                    } else if (strcmp(typeName, "Event") == 0 || strcmp(typeName, "Mutant") == 0 || strcmp(typeName, "Semaphore") == 0) {
                        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%s", typeName);
                    } else if (strcmp(typeName, "Section") == 0 || strcmp(typeName, "Token") == 0) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", typeName);
                    } else if (strcmp(typeName, "ALPC Port") == 0) {
                        ImGui::TextColored(ImVec4(0.8f, 0.6f, 1.0f, 1.0f), "%s", typeName);
                    } else {
                        ImGui::Text("%s", typeName);
                    }
                } else {
                    ImGui::TextDisabled("Type %d", types[i].index);
                }
                
                ImGui::TableSetColumnIndex(2);
                // Show percentage too
                float pct = (stats->TotalHandles > 0) ? (float)types[i].count * 100.0f / (float)stats->TotalHandles : 0;
                ImGui::Text("%s", Core_FormatNumber(types[i].count, buf, sizeof(buf)));
                if (pct >= 1.0f) {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%.0f%%)", pct);
                }
            }
            
            ImGui::EndTable();
        }
        ImGui::EndChild();
        
        ImGui::SameLine();
        
        // Right column: All Processes with expandable details
        ImGui::BeginChild("AllProcessesCol", ImVec2(0, 0), true);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "All Processes (click for details)");
        ImGui::SameLine();
        ImGui::TextDisabled("- sorted by handle count");
        ImGui::Separator();
        
        if (stats->Processes && ImGui::BeginTable("allprocs", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp)) {
            
            ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 50);
            ImGui::TableSetupColumn("Process", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Types", ImGuiTableColumnFlags_WidthFixed, 180);
            ImGui::TableSetupColumn("Total", ImGuiTableColumnFlags_WidthFixed, 70);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            
            for (uint32_t i = 0; i < stats->ProcessCount; i++) {
                HANDLE_PROCESS_INFO* proc = &stats->Processes[i];
                
                ImGui::TableNextRow();
                
                // Make row selectable
                ImGui::TableSetColumnIndex(0);
                char selectId[32];
                snprintf(selectId, sizeof(selectId), "%u##sel%u", proc->ProcessId, i);
                if (ImGui::Selectable(selectId, false, ImGuiSelectableFlags_SpanAllColumns)) {
                    g_SelectedHandleProcIdx = i;
                    g_ShowHandleDetails = true;
                    Core_RefreshHandleDetails(&g_HandleDetails, proc->ProcessId, 0xFFFF);
                }
                
                ImGui::TableSetColumnIndex(1);
                char narrowName[64];
                wcstombs(narrowName, proc->ProcessName, sizeof(narrowName));
                
                // Highlight heavy handle users
                if (proc->HandleCount > 5000) {
                    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", narrowName);
                } else if (proc->HandleCount > 1000) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", narrowName);
                } else {
                    ImGui::Text("%s", narrowName);
                }
                
                ImGui::TableSetColumnIndex(2);
                // Mini type breakdown
                char typeBreak[80] = "";
                int offset = 0;
                if (proc->FileHandles > 0) {
                    offset += snprintf(typeBreak + offset, sizeof(typeBreak) - offset, "F:%u ", proc->FileHandles);
                }
                if (proc->KeyHandles > 0) {
                    offset += snprintf(typeBreak + offset, sizeof(typeBreak) - offset, "K:%u ", proc->KeyHandles);
                }
                if (proc->EventHandles > 0) {
                    offset += snprintf(typeBreak + offset, sizeof(typeBreak) - offset, "E:%u ", proc->EventHandles);
                }
                if (proc->ThreadHandles > 0) {
                    offset += snprintf(typeBreak + offset, sizeof(typeBreak) - offset, "T:%u ", proc->ThreadHandles);
                }
                if (proc->SectionHandles > 0) {
                    offset += snprintf(typeBreak + offset, sizeof(typeBreak) - offset, "S:%u ", proc->SectionHandles);
                }
                ImGui::TextDisabled("%s", typeBreak);
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%s", Core_FormatNumber(proc->HandleCount, buf, sizeof(buf)));
            }
            
            ImGui::EndTable();
        }
        ImGui::EndChild();
    }
    
    ImGui::EndChild();
}

//
// Kernel Drivers Panel
//

static void DrawKernelDriversPanel() {
    KERNEL_DRIVER_LIST* list = &g_State.DriverList;
    char buf[64];
    
    ImGui::BeginChild("DriversPanel", ImVec2(0, 0), true);
    ImGui::Text("LOADED KERNEL DRIVERS");
    ImGui::Separator();
    
    // Refresh button
    if (ImGui::Button("Refresh")) {
        Core_RefreshKernelDrivers(list);
    }
    ImGui::SameLine();
    ImGui::Text("| Total: %u", list->Count);
    
    ImGui::Spacing();
    
    // Filter
    static char driverFilter[64] = "";
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##drvfilter", "Search...", driverFilter, sizeof(driverFilter));
    
    ImGui::Spacing();
    
    if (list->Count > 0 && list->Drivers != NULL && ImGui::BeginTable("drivers", 5,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
        
        ImGui::TableSetupColumn("Order", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 180);
        ImGui::TableSetupColumn("Base Address", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        char filterLower[64];
        strncpy(filterLower, driverFilter, sizeof(filterLower));
        _strlwr(filterLower);
        
        for (uint32_t i = 0; i < list->Count; i++) {
            KERNEL_DRIVER_INFO* driver = &list->Drivers[i];
            
            // Filter check
            if (filterLower[0] != 0) {
                char nameLower[64];
                strncpy(nameLower, driver->Name, sizeof(nameLower));
                _strlwr(nameLower);
                if (strstr(nameLower, filterLower) == nullptr) continue;
            }
            
            char orderStr[16], baseStr[24], sizeStr[32];
            snprintf(orderStr, sizeof(orderStr), "%u", driver->LoadOrder);
            snprintf(baseStr, sizeof(baseStr), "0x%016llX", driver->ImageBase);
            snprintf(sizeStr, sizeof(sizeStr), "%s", Core_FormatBytes(driver->ImageSize, buf, sizeof(buf)));
            
            ImGui::TableNextRow();
            
            // Highlight ntoskrnl (first driver)
            if (i == 0) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, 
                    ImGui::GetColorU32(ImVec4(0.2f, 0.3f, 0.15f, 1.0f)));
            }
            
            ImGui::TableSetColumnIndex(0);
            CopyableCell(orderStr);
            
            ImGui::TableSetColumnIndex(1);
            if (i == 0) {
                CopyableCellColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), driver->Name);
            } else {
                CopyableCell(driver->Name);
            }
            
            ImGui::TableSetColumnIndex(2);
            CopyableCellColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), baseStr);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", sizeStr);
            
            ImGui::TableSetColumnIndex(4);
            CopyableCell(driver->FullPath);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

//
// Kernel Exports Panel
//

static void DrawKernelExportsPanel() {
    KERNEL_EXPORT_LIST* list = &g_State.ExportList;
    KERNEL_DRIVER_LIST* drivers = &g_State.DriverList;
    static char exportFilter[128] = "";
    
    ImGui::Text("NTOSKRNL EXPORTS");
    ImGui::Separator();
    
    // Refresh button
    if (ImGui::Button("Refresh")) {
        // First ensure drivers are loaded to get ntoskrnl path
        if (drivers->Drivers == NULL || drivers->Count == 0) {
            Core_RefreshKernelDrivers(drivers);
        }
        if (drivers->Drivers != NULL && drivers->Count > 0) {
            Core_RefreshKernelExports(list, drivers, 0);  // Index 0 is always ntoskrnl
        }
    }
    ImGui::SameLine();
    ImGui::Text("| Total: %u", list->Count);
   
    
    // Filter
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint("##expfilter", "Search...", exportFilter, sizeof(exportFilter));
    
    // Export table - use remaining space
    float tableHeight = ImGui::GetContentRegionAvail().y - 4;
    if (list->Count > 0 && list->Exports != NULL && ImGui::BeginTable("exports", 4,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | 
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
        ImVec2(0, tableHeight))) {
        
        ImGui::TableSetupColumn("Ordinal", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("RVA", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Kernel Address", ImGuiTableColumnFlags_WidthFixed, 160);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        char filterLower[128] = "";
        if (exportFilter[0] != '\0') {
            strncpy(filterLower, exportFilter, sizeof(filterLower) - 1);
            _strlwr(filterLower);
        }
        
        uint32_t displayed = 0;
        for (uint32_t i = 0; i < list->Count && displayed < 2000; i++) {
            KERNEL_EXPORT_INFO* exp = &list->Exports[i];
            
            // Filter
            if (filterLower[0] != '\0') {
                char nameLower[128];
                strncpy(nameLower, exp->Name, sizeof(nameLower) - 1);
                nameLower[sizeof(nameLower) - 1] = '\0';
                _strlwr(nameLower);
                if (strstr(nameLower, filterLower) == NULL) continue;
            }
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", exp->Ordinal);
            
            ImGui::TableSetColumnIndex(1);
            // Highlight Nt* and Zw* functions
            if (strncmp(exp->Name, "Nt", 2) == 0) {
                CopyableCellColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), exp->Name);
            } else if (strncmp(exp->Name, "Zw", 2) == 0) {
                CopyableCellColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), exp->Name);
            } else {
                CopyableCell(exp->Name);
            }
            
            ImGui::TableSetColumnIndex(2);
            char rvaStr[16];
            snprintf(rvaStr, sizeof(rvaStr), "0x%08X", exp->RVA);
            CopyableCell(rvaStr);
            
            ImGui::TableSetColumnIndex(3);
            if (exp->IsForwarder) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "-> %s", exp->ForwarderName);
            } else {
                char addrStr[24];
                snprintf(addrStr, sizeof(addrStr), "0x%016llX", exp->KernelAddress);
                CopyableCellColored(ImVec4(0.6f, 0.9f, 1.0f, 1.0f), addrStr);
            }
            
            displayed++;
        }
        
        ImGui::EndTable();
        
        if (displayed >= 2000) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), 
                "Limited to 2000 entries. Use filter.");
        }
    } else if (list->Count == 0) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
            "Click 'Refresh Exports' to load ntoskrnl exports.");
    }
}

//
// Helper: Parse hex string with or without 0x prefix
//
static uint64_t ParseHexInput(const char* input) {
    if (!input || !*input) return 0;
    
    // Skip "0x" or "0X" prefix if present
    const char* ptr = input;
    if (ptr[0] == '0' && (ptr[1] == 'x' || ptr[1] == 'X')) {
        ptr += 2;
    }
    
    return strtoull(ptr, NULL, 16);
}

//
// Physical Memory + PFN Query Panel 
//

static void DrawPhysicalMemoryPanel() {
    PFN_QUERY_RESULTS* results = &g_State.PfnResults;
    PHYSICAL_MEMORY_MAP* map = &g_State.PhysMap;
    char buf[64];
    
    ImGui::BeginChild("PhysMemPanel", ImVec2(0, 0), true);
    ImGui::Text("PHYSICAL MEMORY & PFN DATABASE");
    ImGui::Separator();
    
    //  PHYSICAL MEMORY MAP SECTION 
    ImGui::Text("Physical Memory Map");
    ImGui::Text("Total Physical: %.2f GB (%llu pages)", 
        PagesToGBf(map->TotalPages), map->TotalPages);
    ImGui::SameLine();
    ImGui::Text("| Ranges: %u", map->RangeCount);
    
    if (map->RangeCount == 1 && map->Ranges && map->Ranges[0].StartPage == 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1,0.5f,0.2f,1), "(Fallback mode)");
    }
    
    // Compact memory ranges table
    if (map->RangeCount > 0 && ImGui::BeginTable("ranges", 3, 
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit,
        ImVec2(0, 100))) {
        
        ImGui::TableSetupColumn("Start PFN", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Page Count", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        for (uint32_t i = 0; i < map->RangeCount && i < 10; i++) {
            char pfnStr[32], countStr[32];
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            snprintf(pfnStr, sizeof(pfnStr), "0x%012llX", map->Ranges[i].StartPage);
            CopyableCell(pfnStr);
            
            ImGui::TableSetColumnIndex(1);
            snprintf(countStr, sizeof(countStr), "%llu", map->Ranges[i].PageCount);
            CopyableCell(countStr);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", Core_FormatPages(map->Ranges[i].PageCount, buf, sizeof(buf)));
        }
        
        ImGui::EndTable();
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // = VA TRANSLATION SECTION =
    ImGui::Text("Virtual Address Translation");
    static char vaInputBuf[64] = "";
    static bool translationFailed = false;
    static bool translationFound = false;
    static uint64_t foundPA = 0;
    static uint64_t foundPFN = 0;
    
    ImGui::Text("VA (hex, 0x optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    // Use regular input text - allow any hex chars plus x for 0x prefix
    if (ImGui::InputText("##va_input", vaInputBuf, sizeof(vaInputBuf))) {
        g_State.TranslationValid = false;
        translationFailed = false;
        translationFound = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Translate")) {
        uint64_t va = ParseHexInput(vaInputBuf);
        translationFound = false;
        translationFailed = false;
        
        if (va != 0) {
            // Directly translate using superfetch - no need to query PFNs first
            uint64_t physAddr = 0;
            PFN_INFO pfnInfo = {0};
            
            if (Core_TranslateVirtualAddress((PVOID)va, &physAddr, &pfnInfo)) {
                foundPFN = pfnInfo.PageFrameIndex;
                foundPA = physAddr;
                g_State.LastTranslatedVA = va;
                g_State.LastTranslatedPA = physAddr;
                g_State.TranslationValid = true;
                translationFound = true;
            } else {
                // Fallback: also check cached PFN results if available
                if (results->Count > 0) {
                    uint64_t vaPage = va & ~0xFFFULL;
                    for (uint32_t i = 0; i < results->Count; i++) {
                        if (results->Pages[i].VirtualAddress != 0) {
                            uint64_t resultVaPage = results->Pages[i].VirtualAddress & ~0xFFFULL;
                            if (resultVaPage == vaPage) {
                                foundPFN = results->Pages[i].PageFrameIndex;
                                foundPA = results->Pages[i].PhysicalAddress | (va & 0xFFF);
                                g_State.LastTranslatedVA = va;
                                g_State.LastTranslatedPA = foundPA;
                                g_State.TranslationValid = true;
                                translationFound = true;
                                break;
                            }
                        }
                    }
                }
                if (!translationFound) {
                    translationFailed = true;
                }
            }
        } else {
            translationFailed = true;
        }
    }
    
    if (g_State.TranslationValid && translationFound) {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), 
            "VA: 0x%016llX -> PA: 0x%016llX (PFN: 0x%llX)", 
            g_State.LastTranslatedVA, g_State.LastTranslatedPA, foundPFN);
    } else if (translationFailed) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), 
            "VA not found in superfetch database (page may not be resident)");
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Query PFN range
    static uint64_t queryStartPfn = 0x100;  // Start at PFN 0x100 to skip reserved pages
    static int queryCount = 256;
    
    ImGui::Text("Query PFN Range:");
    
    if (g_State.PhysMap.RangeCount > 0) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Use First Range")) {
            queryStartPfn = g_State.PhysMap.Ranges[0].StartPage;
        }
    }
    
    ImGui::SetNextItemWidth(120);
    ImGui::InputScalar("Start PFN", ImGuiDataType_U64, &queryStartPfn, NULL, NULL, "%llX", 
        ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    ImGui::InputInt("Count", &queryCount);
    if (queryCount < 1) queryCount = 1;
    if (queryCount > 4096) queryCount = 4096;
    ImGui::SameLine();
    static bool queryAttempted = false;
    if (ImGui::Button("Query PFNs")) {
        Core_QueryPfnDatabase(queryStartPfn, queryCount, results);
        queryAttempted = true;
    }
    
    // Show error status
    if (queryAttempted) {
        NTSTATUS err = Core_GetLastSuperfetchError();
        if (err != 0) {
            ImGui::TextColored(ImVec4(1,0.3f,0.3f,1), "Superfetch Error: 0x%08X", err);
            ImGui::TextColored(ImVec4(1,0.7f,0.3f,1), "Note: SysMain service must be running");
        }
    }
    
    ImGui::Spacing();
    
    if (results->Count > 0) {
        ImGui::Text("Query Time: %llu ms | Results: %u", results->QueryTimeMs, results->Count);
        
        if (ImGui::BeginTable("pfnresults", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | 
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
            
            ImGui::TableSetupColumn("PFN", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Physical Addr", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableSetupColumn("Virtual Addr", ImGuiTableColumnFlags_WidthFixed, 140);
            ImGui::TableSetupColumn("Use", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("List", ImGuiTableColumnFlags_WidthFixed, 40);
            ImGui::TableSetupColumn("Flags", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            
            for (uint32_t i = 0; i < results->Count && i < 500; i++) {
                PFN_INFO* info = &results->Pages[i];
                char pfnStr[32], physStr[32], virtStr[32];
                
                ImGui::TableNextRow();
                
                ImGui::TableSetColumnIndex(0);
                snprintf(pfnStr, sizeof(pfnStr), "0x%llX", info->PageFrameIndex);
                CopyableCell(pfnStr);
                
                ImGui::TableSetColumnIndex(1);
                snprintf(physStr, sizeof(physStr), "0x%012llX", info->PhysicalAddress);
                CopyableCell(physStr);
                
                ImGui::TableSetColumnIndex(2);
                if (info->VirtualAddress) {
                    snprintf(virtStr, sizeof(virtStr), "0x%012llX", info->VirtualAddress);
                    CopyableCell(virtStr);
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "N/A");
                }
                
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%u", info->UseDescription);
                
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%u", info->ListDescription);
                
                ImGui::TableSetColumnIndex(5);
                const char* flags = "";
                if (info->Pinned && info->Image) flags = "Pin,Img";
                else if (info->Pinned) flags = "Pinned";
                else if (info->Image) flags = "Image";
                ImGui::Text("%s P%u", flags, info->Priority);
            }
            
            ImGui::EndTable();
        }
    }
    
    ImGui::EndChild();
}

//
// Kernel Pointers Panel - EPROCESS, KTHREAD traversal
//

// Thread info structure
typedef struct {
    ULONG ThreadId;
    ULONG OwnerPid;
    uint64_t KThread;
    int Priority;
} THREAD_KERNEL_INFO;

// Process with threads
typedef struct {
    KERNEL_OBJECT_INFO Process;
    THREAD_KERNEL_INFO* Threads;
    uint32_t ThreadCount;
    bool Expanded;
} PROCESS_THREAD_INFO;

static PROCESS_THREAD_INFO* g_ProcessThreads = nullptr;
static uint32_t g_ProcessThreadCount = 0;
static int g_SelectedPid = -1;

// Progressive loading state
static bool g_LoadingProcesses = false;
static bool g_InitialLoadTriggered = false;  // Auto-load on first render
static uint32_t g_LoadingIndex = 0;
static uint32_t g_LoadingTotal = 0;
static PROCESSENTRY32W* g_LoadingBuffer = nullptr;

static void FreeProcessThreads() {
    if (g_ProcessThreads) {
        for (uint32_t i = 0; i < g_ProcessThreadCount; i++) {
            if (g_ProcessThreads[i].Threads) {
                free(g_ProcessThreads[i].Threads);
            }
        }
        free(g_ProcessThreads);
        g_ProcessThreads = nullptr;
    }
    g_ProcessThreadCount = 0;
}

static void EnumerateProcessThreads(PROCESS_THREAD_INFO* procInfo) {
    if (procInfo->Threads) {
        free(procInfo->Threads);
        procInfo->Threads = nullptr;
    }
    procInfo->ThreadCount = 0;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;
    
    // Count threads for this process
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    uint32_t count = 0;
    
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == procInfo->Process.ProcessId) {
                count++;
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    
    if (count == 0) {
        CloseHandle(hSnapshot);
        return;
    }
    
    procInfo->Threads = (THREAD_KERNEL_INFO*)malloc(count * sizeof(THREAD_KERNEL_INFO));
    if (!procInfo->Threads) {
        CloseHandle(hSnapshot);
        return;
    }
    
    // Get thread info
    te.dwSize = sizeof(te);
    if (Thread32First(hSnapshot, &te)) {
        do {
            if (te.th32OwnerProcessID == procInfo->Process.ProcessId && 
                procInfo->ThreadCount < count) {
                THREAD_KERNEL_INFO* ti = &procInfo->Threads[procInfo->ThreadCount];
                ti->ThreadId = te.th32ThreadID;
                ti->OwnerPid = te.th32OwnerProcessID;
                ti->Priority = te.tpBasePri;
                ti->KThread = Core_GetKThread(te.th32OwnerProcessID, te.th32ThreadID);
                procInfo->ThreadCount++;
            }
        } while (Thread32Next(hSnapshot, &te));
    }
    
    CloseHandle(hSnapshot);
}

static void DrawKernelPointersPanel() {
    ImGui::BeginChild("KernelPtrs", ImVec2(0, 0), true);
    
    // Header with current process quick info
    ImGui::Text("KERNEL OBJECT BROWSER");
    ImGui::Separator();
    
    // Current process info - compact
    static uint64_t myEProcess = 0, myKThread = 0;
    static bool queriedSelf = false;
    if (!queriedSelf) {
        myEProcess = Core_GetCurrentEProcess();
        myKThread = Core_GetCurrentKThread();
        queriedSelf = true;
    }
    
    char eStr[24], kStr[24], pidStr[16];
    snprintf(eStr, sizeof(eStr), "0x%016llX", myEProcess);
    snprintf(kStr, sizeof(kStr), "0x%016llX", myKThread);
    snprintf(pidStr, sizeof(pidStr), "%u", GetCurrentProcessId());
    
    ImGui::Text("This Process - PID: ");
    ImGui::SameLine(0, 0);
    CopyableCell(pidStr);
    ImGui::SameLine();
    ImGui::Text("EPROCESS: ");
    ImGui::SameLine(0, 0);
    CopyableCellColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), eStr);
    ImGui::SameLine();
    ImGui::Text("KTHREAD: ");
    ImGui::SameLine(0, 0);
    CopyableCellColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), kStr);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Controls - Progressive loading to avoid freezing
    // Auto-load on first render
    bool shouldLoad = false;
    if (!g_InitialLoadTriggered && !g_LoadingProcesses && g_ProcessThreadCount == 0) {
        g_InitialLoadTriggered = true;
        shouldLoad = true;
    }
    
    if (!g_LoadingProcesses) {
        if (ImGui::Button("Refresh All Processes") || shouldLoad) {
            FreeProcessThreads();
            if (g_LoadingBuffer) { free(g_LoadingBuffer); g_LoadingBuffer = nullptr; }
            
            HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnapshot != INVALID_HANDLE_VALUE) {
                // Count processes
                PROCESSENTRY32W pe;
                pe.dwSize = sizeof(pe);
                uint32_t count = 0;
                if (Process32FirstW(hSnapshot, &pe)) {
                    do { count++; } while (Process32NextW(hSnapshot, &pe));
                }
                
                // Allocate buffers
                g_LoadingBuffer = (PROCESSENTRY32W*)malloc(count * sizeof(PROCESSENTRY32W));
                g_ProcessThreads = (PROCESS_THREAD_INFO*)calloc(count, sizeof(PROCESS_THREAD_INFO));
                
                if (g_LoadingBuffer && g_ProcessThreads) {
                    // Store all process entries
                    uint32_t idx = 0;
                    pe.dwSize = sizeof(pe);
                    if (Process32FirstW(hSnapshot, &pe)) {
                        do {
                            g_LoadingBuffer[idx++] = pe;
                        } while (Process32NextW(hSnapshot, &pe) && idx < count);
                    }
                    g_LoadingTotal = idx;
                    g_LoadingIndex = 0;
                    g_LoadingProcesses = true;
                }
                CloseHandle(hSnapshot);
            }
        }
    } else {
        // Process a batch each frame (20 per frame for speed while staying responsive)
        uint32_t batchSize = 20;
        for (uint32_t b = 0; b < batchSize && g_LoadingIndex < g_LoadingTotal; b++, g_LoadingIndex++) {
            PROCESSENTRY32W* pe = &g_LoadingBuffer[g_LoadingIndex];
            
            // Skip PID 0 (System Idle Process - not a real process)
            if (pe->th32ProcessID == 0) continue;
            
            PROCESS_THREAD_INFO* pti = &g_ProcessThreads[g_ProcessThreadCount];
            
            // Quick fill without slow kernel query - defer detailed info
            memset(&pti->Process, 0, sizeof(pti->Process));
            pti->Process.ProcessId = pe->th32ProcessID;
            pti->Process.ParentPid = pe->th32ParentProcessID;
            pti->Process.ThreadCount = pe->cntThreads;
            wcsncpy(pti->Process.ImageName, pe->szExeFile, 63);
            pti->Process.Valid = true;
            pti->Process.EProcess = 0; // Will be fetched on expand
            pti->Threads = nullptr;
            pti->ThreadCount = 0;
            pti->Expanded = false;
            g_ProcessThreadCount++;
        }
        
        // Done loading?
        if (g_LoadingIndex >= g_LoadingTotal) {
            g_LoadingProcesses = false;
            if (g_LoadingBuffer) { free(g_LoadingBuffer); g_LoadingBuffer = nullptr; }
        }
        
        // Show progress
        ImGui::ProgressBar((float)g_LoadingIndex / (float)g_LoadingTotal, ImVec2(150, 0));
        ImGui::SameLine();
        ImGui::Text("Loading... %u/%u", g_LoadingIndex, g_LoadingTotal);
    }
    ImGui::SameLine();
    ImGui::Text("%u processes", g_ProcessThreadCount);
    
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    static char filterBuf[64] = "";
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##filter", "Search...", filterBuf, sizeof(filterBuf));
    
    ImGui::Spacing();
    
    // Process tree with expandable threads
    if (g_ProcessThreadCount > 0 && g_ProcessThreads) {
        ImGui::BeginChild("ProcessTree", ImVec2(0, 0), false);
        
        for (uint32_t i = 0; i < g_ProcessThreadCount; i++) {
            PROCESS_THREAD_INFO* pti = &g_ProcessThreads[i];
            if (!pti->Process.Valid) continue;
            
            char narrowName[64];
            wcstombs(narrowName, pti->Process.ImageName, sizeof(narrowName));
            
            // Filter check
            if (filterBuf[0] != 0) {
                char nameLower[64], filterLower[64];
                strncpy(nameLower, narrowName, sizeof(nameLower));
                strncpy(filterLower, filterBuf, sizeof(filterLower));
                _strlwr(nameLower);
                _strlwr(filterLower);
                if (strstr(nameLower, filterLower) == nullptr) {
                    // Also check PID
                    char pidCheck[16];
                    snprintf(pidCheck, sizeof(pidCheck), "%u", pti->Process.ProcessId);
                    if (strstr(pidCheck, filterBuf) == nullptr) continue;
                }
            }
            
            char treeLabel[128];
            snprintf(treeLabel, sizeof(treeLabel), "[%u] %s", pti->Process.ProcessId, narrowName);
            
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (g_SelectedPid == (int)pti->Process.ProcessId) {
                flags |= ImGuiTreeNodeFlags_Selected;
            }
            
            bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)pti->Process.ProcessId, flags, "%s", treeLabel);
            
            if (ImGui::IsItemClicked()) {
                g_SelectedPid = pti->Process.ProcessId;
            }
            
            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                char eprocCopy[24];
                snprintf(eprocCopy, sizeof(eprocCopy), "0x%016llX", pti->Process.EProcess);
                if (ImGui::MenuItem("Copy EPROCESS")) {
                    ImGui::SetClipboardText(eprocCopy);
                }
                if (ImGui::MenuItem("Copy PID")) {
                    char pidCopy[16];
                    snprintf(pidCopy, sizeof(pidCopy), "%u", pti->Process.ProcessId);
                    ImGui::SetClipboardText(pidCopy);
                }
                if (ImGui::MenuItem("Copy Name")) {
                    ImGui::SetClipboardText(narrowName);
                }
                ImGui::EndPopup();
            }
            
            if (nodeOpen) {
                // Load threads and EPROCESS on expand (deferred loading)
                if (!pti->Expanded) {
                    // Fetch detailed kernel info now
                    if (pti->Process.EProcess == 0) {
                        Core_GetKernelProcessInfo(pti->Process.ProcessId, &pti->Process);
                    }
                    EnumerateProcessThreads(pti);
                    pti->Expanded = true;
                }
                
                ImGui::Indent();
                
                // Row 1: EPROCESS and PEB
                char eprocessStr[24], pebStr[24];
                snprintf(eprocessStr, sizeof(eprocessStr), "0x%016llX", pti->Process.EProcess);
                snprintf(pebStr, sizeof(pebStr), "0x%016llX", pti->Process.Peb);
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "EPROCESS:");
                ImGui::SameLine();
                CopyableCellColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), eprocessStr);
                if (pti->Process.Peb != 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "PEB:");
                    ImGui::SameLine();
                    CopyableCellColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), pebStr);
                }
                
                // Row 2: Session, Parent, Handles, Threads
                char sessionStr[16], parentStr[16], handleStr[16], threadStr[16];
                snprintf(sessionStr, sizeof(sessionStr), "%u", pti->Process.SessionId);
                snprintf(parentStr, sizeof(parentStr), "%u", pti->Process.ParentPid);
                snprintf(handleStr, sizeof(handleStr), "%u", pti->Process.HandleCount);
                snprintf(threadStr, sizeof(threadStr), "%u", pti->Process.ThreadCount);
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Session:");
                ImGui::SameLine();
                CopyableCell(sessionStr);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Parent:");
                ImGui::SameLine();
                CopyableCell(parentStr);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Handles:");
                ImGui::SameLine();
                CopyableCell(handleStr);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Threads:");
                ImGui::SameLine();
                CopyableCell(threadStr);
                
                // Flags
                if (pti->Process.IsWow64 || pti->Process.IsProtected) {
                    if (pti->Process.IsWow64) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "[WOW64]");
                    }
                    if (pti->Process.IsProtected) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[PROTECTED]");
                    }
                }
                
                // Threads
                if (pti->ThreadCount > 0) {
                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Threads (%u):", pti->ThreadCount);
                    
                    if (ImGui::BeginTable("threads", 4, 
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit)) {
                        
                        ImGui::TableSetupColumn("TID", ImGuiTableColumnFlags_WidthFixed, 60);
                        ImGui::TableSetupColumn("KTHREAD", ImGuiTableColumnFlags_WidthFixed, 160);
                        ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed, 60);
                        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
                        ImGui::TableHeadersRow();
                        
                        for (uint32_t t = 0; t < pti->ThreadCount; t++) {
                            THREAD_KERNEL_INFO* ti = &pti->Threads[t];
                            
                            char tidStr[16], kthreadStr[24], prioStr[8];
                            snprintf(tidStr, sizeof(tidStr), "%u", ti->ThreadId);
                            snprintf(kthreadStr, sizeof(kthreadStr), "0x%016llX", ti->KThread);
                            snprintf(prioStr, sizeof(prioStr), "%d", ti->Priority);
                            
                            ImGui::TableNextRow();
                            
                            ImGui::TableSetColumnIndex(0);
                            CopyableCell(tidStr);
                            
                            ImGui::TableSetColumnIndex(1);
                            if (ti->KThread != 0) {
                                CopyableCellColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), kthreadStr);
                            } else {
                                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Access Denied");
                            }
                            
                            ImGui::TableSetColumnIndex(2);
                            CopyableCell(prioStr);
                            
                            ImGui::TableSetColumnIndex(3);
                            // Empty stretch column
                        }
                        
                        ImGui::EndTable();
                    }
                } else if (pti->Expanded) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No threads accessible");
                }
                
                ImGui::Unindent();
                ImGui::TreePop();
            }
        }
        
        ImGui::EndChild();
    }
    
    ImGui::EndChild();
}

//
// Performance Stats Panel
//

static void DrawPerformancePanel() {
    PERF_STATS* stats = &g_State.PerfStats;
    char buf[64];
    
    ImGui::BeginChild("PerfPanel", ImVec2(0, 0), true);
    ImGui::Text("PROCESSOR & I/O PERFORMANCE");
    ImGui::Separator();
    
    if (!stats->Valid) {
        Core_RefreshPerformanceStats(stats);
    }
    
    // I/O Stats
    ImGui::Text("I/O Statistics:");
    ImGui::Columns(3, "iocols", false);
    ImGui::Text("Read: %s (%u ops)", 
        Core_FormatBytes(stats->IoReadBytes, buf, sizeof(buf)), stats->IoReadOps);
    ImGui::NextColumn();
    ImGui::Text("Write: %s (%u ops)", 
        Core_FormatBytes(stats->IoWriteBytes, buf, sizeof(buf)), stats->IoWriteOps);
    ImGui::NextColumn();
    ImGui::Text("Other: %s (%u ops)", 
        Core_FormatBytes(stats->IoOtherBytes, buf, sizeof(buf)), stats->IoOtherOps);
    ImGui::Columns(1);
    
    ImGui::Spacing();
    ImGui::Separator();
    
    // Per-processor table
    ImGui::Text("Per-Processor Statistics (%u CPUs):", stats->ProcessorCount);
    
    if (stats->ProcessorCount > 0 && ImGui::BeginTable("cpustats", 5,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("CPU", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Kernel Time", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("User Time", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("DPC Time", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Interrupts", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        for (uint32_t i = 0; i < stats->ProcessorCount && i < 128; i++) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", i);
            
            ImGui::TableSetColumnIndex(1);
            // Convert 100ns units to seconds
            ImGui::Text("%.2fs", (double)stats->KernelTime[i] / 10000000.0);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.2fs", (double)stats->UserTime[i] / 10000000.0);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2fs", (double)stats->DpcTime[i] / 10000000.0);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%s", Core_FormatNumber(stats->InterruptCount[i], buf, sizeof(buf)));
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

//
// Process Table
//

static void DrawProcessTable() {
    PROCESS_LIST* list = &g_State.ProcList;
    char buf[64];
    
    ImGui::BeginChild("ProcTable", ImVec2(0, 0), true);
    ImGui::Text("PROCESS MEMORY USAGE");
    ImGui::Separator();
    
    // Filter
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("Filter", g_State.SearchFilter, sizeof(g_State.SearchFilter));
    ImGui::SameLine();
    ImGui::Text("Showing %u processes", list->Count);
    
    ImGui::Spacing();
    
    if (ImGui::BeginTable("processes", 5, 
        ImGuiTableFlags_Sortable | ImGuiTableFlags_RowBg | 
        ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable)) {
        
        ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Working Set", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Private", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Page Faults", ImGuiTableColumnFlags_WidthFixed, 90);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        
        // Filter and display
        char filterLower[256];
        strncpy(filterLower, g_State.SearchFilter, sizeof(filterLower));
        _strlwr(filterLower);
        
        for (uint32_t i = 0; i < list->Count && i < 200; i++) {
            PROCESS_MEMORY_INFO* proc = &list->Processes[i];
            
            // Filter check
            if (filterLower[0] != 0) {
                char nameLower[128];
                wcstombs(nameLower, proc->Name, sizeof(nameLower));
                _strlwr(nameLower);
                if (strstr(nameLower, filterLower) == nullptr) continue;
            }
            
            char pidStr[32];
            char narrowName[64];
            wcstombs(narrowName, proc->Name, sizeof(narrowName));
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            snprintf(pidStr, sizeof(pidStr), "%lu", proc->Pid);
            CopyableCell(pidStr);
            
            ImGui::TableSetColumnIndex(1);
            CopyableCell(narrowName);
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", Core_FormatBytes(proc->WorkingSet, buf, sizeof(buf)));
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%s", Core_FormatBytes(proc->PrivateBytes, buf, sizeof(buf)));
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", proc->PageFaults);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

//
// Prefetch Panel
//

static void DrawPrefetchPanel() {
    PREFETCH_LIST* list = &g_State.PrefetchList;
    char buf[64];
    
    ImGui::BeginChild("PrefetchPanel", ImVec2(0, 0), true);
    ImGui::Text("PREFETCH / LAUNCH HISTORY");
    ImGui::Separator();
    
    ImGui::Text("%u prefetch entries", list->Count);
    ImGui::Spacing();
    
    if (ImGui::BeginTable("prefetch", 3,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("Application", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Last Access", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableHeadersRow();
        
        for (uint32_t i = 0; i < list->Count && i < 100; i++) {
            PREFETCH_ENTRY* entry = &list->Entries[i];
            char narrowName[64];
            wcstombs(narrowName, entry->CleanName, sizeof(narrowName));
            
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            CopyableCell(narrowName);
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", Core_FormatBytes(entry->Size, buf, sizeof(buf)));
            
            ImGui::TableSetColumnIndex(2);
            // Convert FILETIME to readable
            FILETIME ft;
            ft.dwLowDateTime = (DWORD)(entry->LastAccess & 0xFFFFFFFF);
            ft.dwHighDateTime = (DWORD)(entry->LastAccess >> 32);
            SYSTEMTIME st;
            FileTimeToSystemTime(&ft, &st);
            ImGui::Text("%04d-%02d-%02d %02d:%02d", 
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

//
// Main UI Render
//

static void RenderUI() {
    // Update history
    if (g_State.MemStats.Valid) {
        float totalPages = (float)(g_State.MemStats.TotalPhysical / PAGE_SIZE);
        if (totalPages > 0) {
            g_State.StandbyHistory[g_State.HistoryIndex] = 
                (float)g_State.MemStats.TotalStandby / totalPages * 100.0f;
            g_State.FreeHistory[g_State.HistoryIndex] = 
                (float)g_State.MemStats.TotalFree / totalPages * 100.0f;
            g_State.HistoryIndex = (g_State.HistoryIndex + 1) % 120;
        }
    }
    
    // Main window
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)viewport->Size.x, (float)viewport->Size.y));
    
    ImGuiWindowFlags windowFlags = 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("NTMemory ", nullptr, windowFlags);
    
    // Custom title bar - start at absolute 0
    ImGui::SetCursorPos(ImVec2(8, 2));
    
    // Custom title bar
    ImVec2 titleBarMin = ImGui::GetCursorScreenPos();
    ImVec2 titleBarMax = ImVec2(titleBarMin.x + ImGui::GetWindowWidth() - 16, titleBarMin.y + 28);
    
    ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "NTMemory");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Usermode NT Explorer");
    
    // Window controls on right side
    float btnWidth = 28.0f;
    float rightPad = 8.0f;
    //ImGui::SameLine(ImGui::GetWindowWidth() - (btnWidth * 3) - rightPad - 100);
   // ImGui::Text("Updates: %u", g_State.UpdateCount);
    
    ImGui::SameLine(ImGui::GetWindowWidth() - (btnWidth * 3) - rightPad);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.45f, 1.0f));
    
    if (ImGui::Button("_", ImVec2(btnWidth, 24))) {
        ShowWindow(g_hWnd, SW_MINIMIZE);
    }
    ImGui::SameLine(0, 0);
    if (ImGui::Button(g_Maximized ? "=" : "[]", ImVec2(btnWidth, 24))) {
        ShowWindow(g_hWnd, g_Maximized ? SW_RESTORE : SW_MAXIMIZE);
    }
    ImGui::SameLine(0, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.1f, 0.1f, 1.0f));
    if (ImGui::Button("X", ImVec2(btnWidth, 24))) {
        g_Running = false;
    }
    ImGui::PopStyleColor(5);
    
    // Handle title bar drag
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseHoveringRect(titleBarMin, titleBarMax) && !ImGui::IsAnyItemHovered()) {
        if (ImGui::IsMouseClicked(0)) {
            g_Dragging = true;
            POINT pt;
            GetCursorPos(&pt);
            g_DragStart = pt;
        }
        if (ImGui::IsMouseDoubleClicked(0)) {
            ShowWindow(g_hWnd, g_Maximized ? SW_RESTORE : SW_MAXIMIZE);
        }
    }
    if (g_Dragging) {
        if (ImGui::IsMouseDown(0)) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rc;
            GetWindowRect(g_hWnd, &rc);
            int dx = pt.x - g_DragStart.x;
            int dy = pt.y - g_DragStart.y;
            SetWindowPos(g_hWnd, NULL, rc.left + dx, rc.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            g_DragStart = pt;
        } else {
            g_Dragging = false;
        }
    }
    
    ImGui::Separator();
    
    // Add padding for tab content
    ImGui::SetCursorPosX(8);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    
    if (ImGui::BeginTabBar("MainTabs")) {
        
        if (ImGui::BeginTabItem("Overview")) {
            float availHeight = ImGui::GetContentRegionAvail().y - 8;
            float leftCol1 = availHeight * 0.42f;
            float leftCol2 = availHeight * 0.32f;
            float leftCol3 = availHeight * 0.26f;
            float rightCol1 = availHeight * 0.42f;
            float rightCol2 = availHeight * 0.22f;
            float rightCol3 = availHeight * 0.36f;
            
            ImGui::Columns(2, "overview_cols", false);

            // Left column
            ImGui::BeginChild("MemOverviewWrap", ImVec2(0, leftCol1), false);
            DrawMemoryOverviewInner();
            ImGui::EndChild();
            
            ImGui::BeginChild("MemListsWrap", ImVec2(0, leftCol2), false);
            DrawMemoryListsInner();
            ImGui::EndChild();
            
            ImGui::BeginChild("HistoryWrap", ImVec2(0, leftCol3), false);
            DrawHistoryGraphInner();
            ImGui::EndChild();

            ImGui::NextColumn();

            // Right column
            ImGui::BeginChild("StandbyWrap", ImVec2(0, rightCol1), false);
            DrawStandbyPanelInner();
            ImGui::EndChild();
            
            ImGui::BeginChild("CompressionWrap", ImVec2(0, rightCol2), false);
            DrawCompressionPanelInner();
            ImGui::EndChild();
            
            ImGui::BeginChild("SummaryWrap", ImVec2(0, rightCol3), false);
            DrawSystemSummaryInner();
            ImGui::EndChild();

            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        //  KERNEL / LOW-LEVEL 
        if (ImGui::BeginTabItem("Kernel Objects")) {
            DrawKernelPointersPanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Kernel Drivers")) {
            // Refresh drivers when tab is selected
            static uint64_t lastDriverRefresh = 0;
            uint64_t now = Core_GetTickCount64();
            if (now - lastDriverRefresh > 10000 || g_State.DriverList.Count == 0) {
                Core_RefreshKernelDrivers(&g_State.DriverList);
                lastDriverRefresh = now;
            }
            DrawKernelDriversPanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Ntoskrnl Exports")) {
            DrawKernelExportsPanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Physical Memory")) {
            DrawPhysicalMemoryPanel();
            ImGui::EndTabItem();
        }
        
        //  PROCESSES / MEMORY 
        if (ImGui::BeginTabItem("Processes")) {
            DrawProcessTable();
            ImGui::EndTabItem();
        }
        
        //  SYSTEM INFO 
        if (ImGui::BeginTabItem("Pool Tags")) {
            // Refresh pool tags when tab is selected
            static uint64_t lastPoolRefresh = 0;
            uint64_t now = Core_GetTickCount64();
            if (now - lastPoolRefresh > 5000) {  // Refresh every 5 seconds
                Core_RefreshPoolTags(&g_State.PoolTags);
                lastPoolRefresh = now;
            }
            DrawPoolTagsPanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Handles")) {
            // Refresh handles when tab is selected
            static uint64_t lastHandleRefresh = 0;
            uint64_t now = Core_GetTickCount64();
            if (now - lastHandleRefresh > 5000) {
                Core_RefreshHandleStats(&g_State.HandleStats);
                lastHandleRefresh = now;
            }
            DrawHandleStatsPanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Performance")) {
            Core_RefreshPerformanceStats(&g_State.PerfStats);
            DrawPerformancePanel();
            ImGui::EndTabItem();
        }
        
        if (ImGui::BeginTabItem("Prefetch")) {
            DrawPrefetchPanel();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }
    
    ImGui::PopStyleVar(2);  // WindowPadding, WindowBorderSize
    ImGui::End();
}

//
// DirectX Setup
//

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { 
        D3D_FEATURE_LEVEL_11_0, 
        D3D_FEATURE_LEVEL_10_0 
    };
    
    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 
        createDeviceFlags, featureLevelArray, 2,
        D3D11_SDK_VERSION, &sd, &g_pSwapChain, 
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            createDeviceFlags, featureLevelArray, 2,
            D3D11_SDK_VERSION, &sd, &g_pSwapChain,
            &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    }
    
    if (FAILED(res)) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { 
        g_mainRenderTargetView->Release(); 
        g_mainRenderTargetView = nullptr; 
    }
}

//
// Window Procedure
//

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_NCCALCSIZE:
        // Remove non-client area (title bar, borders) entirely
        if (wParam == TRUE) {
            return 0;
        }
        break;
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), 
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        g_Maximized = (wParam == SIZE_MAXIMIZED);
        return 0;
    case WM_NCHITTEST: {
        // Allow resize from edges
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int border = 6;
        
        // Check corners first
        if (pt.y < border && pt.x < border) return HTTOPLEFT;
        if (pt.y < border && pt.x > rc.right - border) return HTTOPRIGHT;
        if (pt.y > rc.bottom - border && pt.x < border) return HTBOTTOMLEFT;
        if (pt.y > rc.bottom - border && pt.x > rc.right - border) return HTBOTTOMRIGHT;
        
        // Then edges
        if (pt.y < border) return HTTOP;
        if (pt.y > rc.bottom - border) return HTBOTTOM;
        if (pt.x < border) return HTLEFT;
        if (pt.x > rc.right - border) return HTRIGHT;
        
        return HTCLIENT;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        g_Running = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

//
// Background Refresh Thread
//

static DWORD WINAPI RefreshThread(LPVOID param) {
    (void)param;
    
    while (g_Running && g_State.AutoRefresh) {
        Core_RefreshMemoryStats(&g_State.MemStats);
        
        // Less frequent process refresh (expensive)
        static int counter = 0;
        if (++counter % 5 == 0) {
            Core_RefreshProcessList(&g_State.ProcList);
        }
        
        // Refresh compression stats every 10 cycles
        if (counter % 10 == 0) {
            Core_RefreshCompressionStats(&g_State.CompressionStats);
        }
        
        g_State.UpdateCount++;
        Sleep(g_State.RefreshIntervalMs);
    }
    
    return 0;
}

//
// Entry Point
//

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Check admin rights
    if (!Core_IsElevated()) {
        MessageBoxW(NULL, 
            L"This application requires administrator privileges.\n"
            L"Please run as Administrator.",
            L"NTMemory", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Initialize core
    if (!Core_Initialize()) {
        MessageBoxW(NULL, 
            L"Failed to initialize core components.",
            L"NTMemory", MB_ICONERROR | MB_OK);
        return 1;
    }
    
    // Initialize state
    g_State.RefreshIntervalMs = 1000;
    g_State.AutoRefresh = true;
    g_State.DarkMode = true;
    g_State.ShowGraphs = true;
    
    // Create window
    WNDCLASSEXW wc = { 
        sizeof(wc), CS_CLASSDC, WndProc, 0, 0, 
        hInstance, nullptr, nullptr, nullptr, nullptr, 
        L"NTMemory", nullptr 
    };
    RegisterClassExW(&wc);
    
    g_hWnd = CreateWindowW(
        wc.lpszClassName, L"NTMemory",
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
        100, 100, 1280, 800,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!CreateDeviceD3D(g_hWnd)) {
        CleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Disable imgui.ini
    
    SetupImGuiStyle();
    
    ImGui_ImplWin32_Init(g_hWnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    
    // Initial data load
    Core_RefreshMemoryStats(&g_State.MemStats);
    Core_RefreshProcessList(&g_State.ProcList);
    Core_RefreshPhysicalMap(&g_State.PhysMap);
    Core_RefreshPrefetchList(&g_State.PrefetchList);
    Core_RefreshCompressionStats(&g_State.CompressionStats);
    Core_RefreshPerformanceStats(&g_State.PerfStats);
    Core_RefreshKernelDrivers(&g_State.DriverList);  // Load kernel drivers on startup
    
    // Start refresh thread
    HANDLE hRefreshThread = CreateThread(nullptr, 0, RefreshThread, nullptr, 0, nullptr);
    
    // Main loop
    ImVec4 clearColor = ImVec4(0.04f, 0.04f, 0.06f, 1.00f);
    
    while (g_Running) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                g_Running = false;
        } 
        
        if (!g_Running) break;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, (float*)&clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        
        // VSync
        g_pSwapChain->Present(1, 0);
    }
    
    // Cleanup
    g_State.AutoRefresh = false;
    if (hRefreshThread) {
        WaitForSingleObject(hRefreshThread, 2000);
        CloseHandle(hRefreshThread);
    }
    
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(g_hWnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    
    // Free allocated memory
    free(g_State.ProcList.Processes);
    free(g_State.PhysMap.Ranges);
    free(g_State.PrefetchList.Entries);
    free(g_State.PoolTags.Tags);
    free(g_State.PfnResults.Pages);
    free(g_State.SuperfetchProcs);
    free(g_State.PerfStats.IdleTime);
    free(g_State.PerfStats.KernelTime);
    free(g_State.PerfStats.UserTime);
    free(g_State.PerfStats.DpcTime);
    free(g_State.PerfStats.InterruptTime);
    free(g_State.PerfStats.InterruptCount);
    free(g_State.PerfStats.DpcCount);
    
    // Free kernel driver and export data
    free(g_State.DriverList.Drivers);
    Core_FreeKernelExports(&g_State.ExportList);
    
    // Free handle stats
    free(g_State.HandleStats.Processes);
    Core_FreeHandleDetails(&g_HandleDetails);
    
    Core_Shutdown();

    return 0;
}