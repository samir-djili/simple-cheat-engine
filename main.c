#pragma pack(push, 8)
#include <windows.h>
#include <tlhelp32.h>
#pragma pack(pop)
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 10

typedef struct {
    LPVOID address;
    int value;
} MemoryEntry;

MemoryEntry* foundAddresses = NULL;
size_t foundCount = 0;
size_t currentPage = 0;
DWORD processID = 0;

// Control handles
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                  LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MemoryScannerClass";

    if (!RegisterClassW(&wc)) return 0;

    HWND hwnd = CreateWindowW(L"MemoryScannerClass", L"Memory Scanner",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             650, 450, NULL, NULL, hInstance, NULL);
    if (!hwnd) return 0;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            // Process selection controls
            hwndProcList = CreateWindowW(L"LISTBOX", NULL,
                            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
                            10, 10, 620, 200, hwnd, (HMENU)1, NULL, NULL);
            hwndSelectBtn = CreateWindowW(L"BUTTON", L"Select Process",
                                        WS_CHILD | WS_VISIBLE,
                                        10, 220, 120, 30, hwnd, (HMENU)2, NULL, NULL);
            hwndExitBtn = CreateWindowW(L"BUTTON", L"Exit", WS_CHILD | WS_VISIBLE,
                                      550, 220, 80, 30, hwnd, (HMENU)3, NULL, NULL);

            // Memory operation controls (initially hidden)
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

            ListProcesses();
            ShowProcessSelection(hwnd);
            break;

        case WM_COMMAND: {
            int cmd = LOWORD(wParam);
            if (cmd == 2) { // Select Process
                int index = SendMessageW(hwndProcList, LB_GETCURSEL, 0, 0);
                if (index != LB_ERR) {
                    wchar_t buffer[256];
                    SendMessageW(hwndProcList, LB_GETTEXT, index, (LPARAM)buffer);

                    wchar_t* pidStart = wcsstr(buffer, L"(PID: ");
                    if (pidStart) {
                        processID = _wtoi(pidStart + 6);
                        ShowMemoryControls(hwnd);
                    }
                }
            }
            else if (cmd == 3) PostQuitMessage(0);
            else if (cmd == 6) { // Scan
                wchar_t buffer[32];
                GetWindowTextW(hwndInput, buffer, 32);
                ScanMemory(_wtoi(buffer));
            }
            else if (cmd == 7) { // Rescan
                wchar_t buffer[32];
                GetWindowTextW(hwndInput, buffer, 32);
                RescanMemory(_wtoi(buffer));
            }
            else if (cmd == 9) { // Return
                ClearResults();
                ShowProcessSelection(hwnd);
            }
            else if (cmd == 14) { // Write Selected
                    int index = SendMessageW(hwndResultList, LB_GETCURSEL, 0, 0);
                    if (index >= 0 && index < (int)foundCount) {
                        wchar_t valueBuffer[32];
                        GetWindowTextW(hwndWriteInput, valueBuffer, 32);
                        int newValue = _wtoi(valueBuffer);

                        HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                                    FALSE, processID);
                        if (hProcess) {
                            SIZE_T bytesWritten;
                            LPVOID address = foundAddresses[index].address;
                            if (WriteProcessMemory(hProcess, address, &newValue,
                                                  sizeof(newValue), &bytesWritten)) {
                                MessageBoxW(hwnd, L"Value written successfully!",
                                          L"Success", MB_OK);
                                // Update local copy of the value
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


            // Update the Write button handler (command 8) to write to all addresses
            else if (cmd == 8) { // Write All
                wchar_t valueBuffer[32];
                GetWindowTextW(hwndWriteInput, valueBuffer, 32);
                int newValue = _wtoi(valueBuffer);

                HANDLE hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_OPERATION,
                                            FALSE, processID);
                if (hProcess) {
                    SIZE_T bytesWritten;
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
            else if (cmd == 10) { // Prev Page
                if (currentPage > 0) currentPage--;
                UpdatePagination();
            }
            else if (cmd == 11) { // Next Page
                if ((currentPage + 1) * PAGE_SIZE < foundCount) currentPage++;
                UpdatePagination();
            }
            break;
        }

        case WM_DESTROY:
            free(foundAddresses);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Update ShowProcessSelection and ShowMemoryControls
void ShowProcessSelection(HWND hwnd) {
    // Show process selection controls
    ShowWindow(hwndProcList, SW_SHOW);
    ShowWindow(hwndSelectBtn, SW_SHOW);
    ShowWindow(hwndExitBtn, SW_SHOW);

    // Hide all memory operation controls
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

    // Refresh the window
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

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

    // Refresh the window
    InvalidateRect(hwnd, NULL, TRUE);
    UpdateWindow(hwnd);
}

void ListProcesses() {
    SendMessageW(hwndProcList, LB_RESETCONTENT, 0, 0);

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnap, &pe)) {
        do {
            // Convert ANSI process name to wide char
            wchar_t wideName[MAX_PATH];
            MultiByteToWideChar(CP_ACP, 0, pe.szExeFile, -1, wideName, MAX_PATH);

            // Create display string with PID
            wchar_t buffer[256];
            swprintf(buffer, 256, L"%s (PID: %d)", pe.szExeFile, pe.th32ProcessID);

            // Add to listbox
            SendMessageW(hwndProcList, LB_ADDSTRING, 0, (LPARAM)buffer);
        } while (Process32Next(hSnap, &pe));
    }
    CloseHandle(hSnap);
}
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

    while (VirtualQueryEx(hProcess, address, &mbi, sizeof(mbi))) {
        if (mbi.State == MEM_COMMIT && (mbi.Protect & PAGE_READWRITE)) {
            BYTE* buffer = (BYTE*)malloc(mbi.RegionSize);
            SIZE_T bytesRead;
            if (ReadProcessMemory(hProcess, mbi.BaseAddress, buffer,
                                 mbi.RegionSize, &bytesRead)) {
                for (SIZE_T i = 0; i < bytesRead; i += sizeof(int)) {
                    if (*(int*)(buffer + i) == targetValue) {
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
        address = (LPVOID)((DWORD_PTR)mbi.BaseAddress + mbi.RegionSize);
    }

    CloseHandle(hProcess);
    currentPage = 0;
    UpdatePagination();
}

void UpdatePagination() {
    SendMessageW(hwndResultList, LB_RESETCONTENT, 0, 0);

    size_t start = currentPage * PAGE_SIZE;
    size_t end = start + PAGE_SIZE;
    if (end > foundCount) end = foundCount;

    for (size_t i = start; i < end; i++) {
        wchar_t buffer[32];
        swprintf(buffer, 32, L"0x%p", foundAddresses[i].address);
        SendMessageW(hwndResultList, LB_ADDSTRING, 0, (LPARAM)buffer);
    }

    int totalPages = (foundCount + PAGE_SIZE - 1) / PAGE_SIZE;
    wchar_t pageInfo[32];
    swprintf(pageInfo, 32, L"Page %d/%d", currentPage + 1, totalPages ? totalPages : 1);
    SetWindowTextW(hwndPageInfo, pageInfo);
}

void ClearResults() {
    free(foundAddresses);
    foundAddresses = NULL;
    foundCount = 0;
    currentPage = 0;
    SendMessageW(hwndResultList, LB_RESETCONTENT, 0, 0);
    SetWindowTextW(hwndPageInfo, L"Page 1/1");
}

void RescanMemory(int newValue) {
    if (!foundCount || !processID) return;

    HANDLE hProcess = OpenProcess(PROCESS_VM_READ, FALSE, processID);
    if (!hProcess) {
        MessageBoxW(NULL, L"Failed to open process!", L"Error", MB_ICONERROR);
        return;
    }

    SIZE_T bytesRead;
    size_t newCount = 0;

    for (size_t i = 0; i < foundCount; i++) {
        int currentValue;
        if (ReadProcessMemory(hProcess, foundAddresses[i].address, &currentValue,
                            sizeof(currentValue), &bytesRead) &&
            currentValue == newValue) {
            foundAddresses[newCount++] = foundAddresses[i];
        }
    }

    foundCount = newCount;
    foundAddresses = (MemoryEntry*)realloc(foundAddresses, foundCount * sizeof(MemoryEntry));
    CloseHandle(hProcess);

    currentPage = 0;
    UpdatePagination();
}
