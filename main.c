/*
Windows Memory Scanner - A simple process memory scanner and editor

Features:
- List running processes
- Select a process to scan
- Scan for integer values in writable memory regions
- Rescan previous results to narrow down values
- Paginated results view
- Write new values to selected/all addresses

Usage:
1. Select a process from the list
2. Enter initial value to scan for
3. Click "Scan" to find all occurrences
4. Refine results with "Rescan" after value changes
5. Write new values using "Write Selected" or "Write All"

Note: This is a basic demonstration - real memory scanners require:
- Better error handling
- Support for different data types
- Cheat engine-like features (fuzzy search, value tracking)
- Memory protection handling
- 64-bit compatibility
*/

// Force 8-byte packing for Windows headers to ensure structure alignment
#pragma pack(push, 8)
#include <windows.h>
#include <tlhelp32.h>
#pragma pack(pop)
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 10  // Number of results to show per page

// Structure to store memory address and value pairs
typedef struct {
    LPVOID address;  // Memory address in target process
    int value;       // Value found at this address
} MemoryEntry;

// Global state variables
MemoryEntry* foundAddresses = NULL;  // Dynamic array of found addresses
size_t foundCount = 0;               // Number of found addresses
size_t currentPage = 0;              // Current pagination page
DWORD processID = 0;                 // Currently selected process ID

// GUI Control handles
HWND hwndProcList, hwndResultList, hwndScanBtn, hwndRescanBtn, hwndWriteBtn,
     hwndExitBtn, hwndInput, hwndNextPage, hwndPrevPage, hwndPageInfo,
     hwndSelectBtn, hwndReturnBtn , hwndWriteInput, hwndWriteSelectedBtn;

// Forward declarations
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void ShowProcessSelection(HWND hwnd);
void ShowMemoryControls(HWND hwnd);
void ListProcesses();
void ScanMemory(int targetValue);
void UpdatePagination();
void ClearResults();
void RescanMemory(int newValue);

// Application entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                  LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MemoryScannerClass";

    if (!RegisterClassW(&wc)) return 0;

    // Create main window
    HWND hwnd = CreateWindowW(L"MemoryScannerClass", L"Memory Scanner",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             650, 450, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// Main window procedure handling messages and events
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Initialize GUI controls
            // Process list and selection controls
            hwndProcList = CreateWindowW(L"LISTBOX", NULL,
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
                            10, 10, 620, 200, hwnd, (HMENU)1, NULL, NULL);
            hwndSelectBtn = CreateWindowW(L"BUTTON", L"Select Process",
                                        WS_CHILD | WS_VISIBLE,
                                        10, 220, 120, 30, hwnd, (HMENU)2, NULL, NULL);
            hwndExitBtn = CreateWindowW(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE,
                                      550, 220, 80, 30, hwnd, (HMENU)3, NULL, NULL);

            // Memory scanning controls (initially hidden)
            hwndResultList = CreateWindowW(L"LISTBOX", NULL, WS_CHILD | WS_VSCROLL,
                                         10, 10, 620, 200, hwnd, (HMENU)4, NULL, NULL);

            // First row of controls (Y = 220)
            hwndInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER,
                                    10, 220, 100, 25, hwnd, (HMENU)5, NULL, NULL);
            hwndScanBtn = CreateWindowW(L"BUTTON", L"Scan", WS_CHILD,
                                      120, 220, 80, 25, hwnd, (HMENU)6, NULL, NULL);
            hwndRescanBtn = CreateWindowW(L"BUTTON", L"Rescan", WS_CHILD,
                                       210, 220, 80, 25, hwnd, (HMENU)7, NULL, NULL);

            // Second row of controls (Y = 250)
            hwndWriteInput = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_BORDER,
                                        10, 250, 100, 25, hwnd, (HMENU)13, NULL, NULL);
            hwndWriteBtn = CreateWindowW(L"BUTTON", L"Write All", WS_CHILD,
                                      120, 250, 80, 25, hwnd, (HMENU)8, NULL, NULL);
            hwndWriteSelectedBtn = CreateWindowW(L"BUTTON", L"Write Selected", WS_CHILD,
                                              210, 250, 120, 25, hwnd, (HMENU)14, NULL, NULL);
            hwndReturnBtn = CreateWindowW(L"BUTTON", L"Return", WS_CHILD,
                                       340, 250, 80, 25, hwnd, (HMENU)9, NULL, NULL);

            // Pagination controls (Y = 280)
            hwndPrevPage = CreateWindowW(L"BUTTON", L"< Prev", WS_CHILD,
                                      10, 280, 80, 25, hwnd, (HMENU)10, NULL, NULL);
            hwndNextPage = CreateWindowW(L"BUTTON", L"Next >", WS_CHILD,
                                      100, 280, 80, 25, hwnd, (HMENU)11, NULL, NULL);
            hwndPageInfo = CreateWindowW(L"STATIC", L"Page 1/1", WS_CHILD,
                                      200, 280, 200, 25, hwnd, (HMENU)12, NULL, NULL);

            // Initial setup
            ListProcesses();
            ShowProcessSelection(hwnd);
            break;

        case WM_COMMAND: {
            int cmd = LOWORD(wParam);
            if (cmd == 2) { // Select Process button
                int index = SendMessageW(hwndProcList, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    wchar_t buffer[256];
                    SendMessageW(hwndProcList, LB_GETTEXT, index, (LPARAM)buffer);

                    // Extract PID from listbox string format: "Process (PID: 1234)"
                    wchar_t* pidStart = wcsstr(buffer, L"(PID: ");
                    if (pidStart) {
                        processID = _wtoi(pidStart + 6);
                        ShowMemoryControls(hwnd);
                    }
                }
            }
            else if (cmd == 3) PostQuitMessage(0); // Exit button
            else if (cmd == 6) { // Scan button
                wchar_t buffer[32];
                GetWindowTextW(hwndInput, buffer, 32);
                ScanMemory(_wtoi(buffer));
            }
            else if (cmd == 7) { // Rescan button
                wchar_t buffer[32];
                GetWindowTextW(hwndInput, buffer, 32);
                RescanMemory(_wtoi(buffer));
            }
            else if (cmd == 9) { // Return to process list
                ClearResults();
                ShowProcessSelection(hwnd);
            }
            else if (cmd == 14) { // Write Selected button
                int index = SendMessageW(hwndResultList, LB_GETCURSEL, 0, 0);
                if (index >= 0 && index < (int)foundCount) {
                    wchar_t valueBuffer[32];
                    GetWindowTextW(hwndWriteInput, valueBuffer, 32);
                    int newValue = _wtoi(valueBuffer);

                    // Open process with write permissions
                    HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                                FALSE, processID);
                    if (hProcess) {
                        SIZE_T bytesWritten;
                        LPVOID address = foundAddresses[index].address;
                        if (WriteProcessMemory(hProcess, address, &newValue,
                                              sizeof(newValue), &bytesWritten)) {
                            MessageBoxW(hwnd, L"Value written successfully!",
                                      L"Success", MB_OK);
                            // Update local copy to maintain consistency
                            foundAddresses[index].value = newValue;
                        }
                        else {
                            MessageBoxW(hwnd, L"Failed to write memory!",
                                      L"Error", MB_ICONERROR);
                        }
                        CloseHandle(hProcess);
                    }
                    else {
                        MessageBoxW(hwnd, L"Failed to open process!",
                                  L"Error", MB_ICONERROR);
                    }
                }
                else {
                    MessageBoxW(hwnd, L"Select an address first!",
                              L"Error", MB_ICONERROR);
                }
            }
            else if (cmd == 8) { // Write All button
                wchar_t valueBuffer[32];
                GetWindowTextW(hwndWriteInput, valueBuffer, 32);
                int newValue = _wtoi(valueBuffer);

                HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                            FALSE, processID);
                if (hProcess) {
                    SIZE_T bytesWritten;
                    // Iterate through all found addresses and update them
                    for (size_t i = 0; i < foundCount; i++) {
                        if (WriteProcessMemory(hProcess, foundAddresses[i].address, &newValue,
                                              sizeof(newValue), &bytesWritten)) {
                            foundAddresses[i].value = newValue;
                        }
                    }
                    CloseHandle(hProcess);
                    MessageBoxW(hwnd, L"All values updated!", L"Success", MB_OK);
                }
                else {
                    MessageBoxW(hwnd, L"Failed to open process!", L"Error", MB_ICONERROR);
                }
            }
            else if (cmd == 10) { // Previous Page button
                if (currentPage > 0) currentPage--;
                UpdatePagination();
            }
            else if (cmd == 11) { // Next Page button
                if ((currentPage + 1) * PAGE_SIZE < foundCount) currentPage++;
                UpdatePagination();
            }
            break;
        }

        case WM_DESTROY:
            // Cleanup dynamically allocated memory
            free(foundAddresses);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Show process selection UI elements
void ShowProcessSelection(HWND hwnd) {
    // Toggle visibility of controls
    ShowWindow(hwndProcList, SW_SHOW);
    ShowWindow(hwndSelectBtn, SW_SHOW);
    ShowWindow(hwndExitBtn, SW_SHOW);

    // Hide memory operation controls
    ShowWindow(hwndResultList, SW_HIDE);
    ShowWindow(hwndInput, SW_HIDE);
    ShowWindow(hwndScanBtn, SW_HIDE);
    ShowWindow(hwndRescanBtn, SW_HIDE);
    ShowWindow(hwndWriteBtn, SW_HIDE);
    ShowWindow(hwndReturnBtn, SW_HIDE);
    ShowWindow(hwndPrevPage, SW_HIDE);
    ShowWindow(hwndNextPage, SW_HIDE);
    ShowWindow(hwndPageInfo, SW_HIDE);
    ShowWindow(hwndWriteInput, SW_HIDE);
    ShowWindow(hwndWriteSelectedBtn, SW_HIDE);

    // Force window redraw
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

// Show memory scanning/editing UI elements
void ShowMemoryControls(HWND hwnd)  {
    // Hide process selection controls
    ShowWindow(hwndProcList, SW_HIDE);
    ShowWindow(hwndSelectBtn, SW_HIDE);
    ShowWindow(hwndExitBtn, SW_SHOW);

    // Show memory operation controls
    ShowWindow(hwndResultList, SW_SHOW);
    ShowWindow(hwndInput, SW_SHOW);
    ShowWindow(hwndScanBtn, SW_SHOW);
    ShowWindow(hwndRescanBtn, SW_SHOW);
    ShowWindow(hwndWriteBtn, SW_SHOW);
    ShowWindow(hwndReturnBtn, SW_SHOW);
    ShowWindow(hwndPrevPage, SW_SHOW);
    ShowWindow(hwndNextPage, SW_SHOW);
    ShowWindow(hwndPageInfo, SW_SHOW);
    ShowWindow(hwndWriteInput, SW_SHOW);
    ShowWindow(hwndWriteSelectedBtn, SW_SHOW);

    // Force window redraw
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

// Populate process list using Toolhelp32 snapshot
void ListProcesses() {
    SendMessageW(hwndProcList, LB_RESETCONTENT, 0, 0);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe)) {
        do {
            // Convert ANSI process name to wide characters
            wchar_t wideName[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, pe.szExeFile, -1, wideName, MAX_PATH);

            // Format process entry with PID
            wchar_t buffer[256];
            swprintf(buffer, 256, L"%s (PID: %d)", pe.szExeFile, pe.th32ProcessID);

            // Add to listbox
            SendMessageW(hwndProcList, LB_ADDSTRING, 0, (LPARAM)buffer);
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
}

// Perform initial memory scan for target value
void ScanMemory(int targetValue) {
    ClearResults();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                 FALSE, processID);
    if (!hProcess) {
        MessageBoxW(NULL, L"Failed to open process!", L"Error", MB_ICONERROR);
        return;
    }

    MEMORY_BASIC_INFORMATION mbi;
    LPVOID address = 0;

    // Iterate through memory regions
    while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi))) {
        // Only scan committed, readable/writable memory
        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_READWRITE)) {
            BYTE* buffer = (BYTE*)malloc(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer,
                                 mbi.RegionSize, &bytesRead)) {
                // Scan each integer in the memory block
                for (SIZE_T i = 0; i < bytesRead; i += sizeof(int)) {
                    if (*(int*)(buffer + i) == targetValue) {
                        // Add matching address to results
                        foundAddresses = (MemoryEntry*)realloc(foundAddresses,
                                          (foundCount + 1) * sizeof(MemoryEntry));
                        foundAddresses[foundCount].address =
                            (LPVOID)((DWORD_PTR)mbi.BaseAddress + i);
                        foundAddresses[foundCount].value = targetValue;
                        foundCount++;
                    }
                }
            }
            free(buffer);
        }
        // Move to next memory region
        address = (LPVOID)((DWORD_PTR)mbi.BaseAddress + mbi.RegionSize);
    }

    CloseHandle(hProcess);
    currentPage = 0;
    UpdatePagination();
}

// Update pagination controls and result display
void UpdatePagination() {
    SendMessageW(hwndResultList, LB_RESETCONTENT, 0, 0);

    // Calculate page boundaries
    size_t start = currentPage * PAGE_SIZE;
    size_t end = start + PAGE_SIZE;
    if (end > foundCount) end = foundCount;

    // Add current page items to listbox
    for (size_t i = start; i < end; i++) {
        wchar_t buffer[32];
        swprintf(buffer, 32, L"0x%p", foundAddresses[i].address);
        SendMessageW(hwndResultList, LB_ADDSTRING, 0, (LPARAM)buffer);
    }

    // Update page info text
    int totalPages = (foundCount + PAGE_SIZE - 1) / PAGE_SIZE;
    wchar_t pageInfo[32];
    swprintf(pageInfo, 32, L"Page %d/%d", currentPage + 1, totalPages ? totalPages : 1);
    SetWindowTextW(hwndPageInfo, pageInfo);
}

// Reset scan results and UI
void ClearResults() {
    free(foundAddresses);
    foundAddresses = NULL;
    foundCount = 0;
    currentPage = 0;
    SendMessageW(hwndResultList, LB_RESETCONTENT, 0, 0);
    SetWindowTextW(hwndPageInfo, L"Page 1/1");
}

// Narrow down results by rescanning previous matches
void RescanMemory(int newValue) {
    if (!foundCount || !processID) return;

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) {
        MessageBoxW(NULL, L"Failed to open process!", L"Error", MB_ICONERROR);
        return;
    }

    SIZE_T bytesRead;
    size_t newCount = 0;

    // Check each previously found address for the new value
    for (size_t i = 0; i < foundCount; i++) {
        int currentValue;
        if (ReadProcessMemory(hProcess, foundAddresses[i].address, &currentValue,
                            sizeof(currentValue), &bytesRead) &&
            currentValue == newValue) {
            // Keep addresses that still match
            foundAddresses[newCount++] = foundAddresses[i];
        }
    }

    // Update results
    foundCount = newCount;
    foundAddresses = (MemoryEntry*)realloc(foundAddresses, foundCount * sizeof(MemoryEntry));
    CloseHandle(hProcess);

    currentPage = 0;
    UpdatePagination();
}
