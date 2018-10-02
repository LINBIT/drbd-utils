/* Compile this with mingw. It won't compile with normal gcc */

#include <windows.h>
// #include <winioctl.h>
// #include <ntsecapi.h>
#include <stdio.h>

#define WINDRBDUM_SVC "WinDRBDUM"

SERVICE_STATUS WinDRBDSvcStatus;
SERVICE_STATUS_HANDLE WinDRBDSvcStatusHandle;
HANDLE WinDRBDSvcStopEvent = NULL;

VOID
CALLBACK
WinDRBDSvcCtrlHandler(DWORD Opcode)
{
    if (Opcode == SERVICE_CONTROL_STOP)
    {
        SetEvent(WinDRBDSvcStopEvent);

        WinDRBDSvcStatus.dwWin32ExitCode = NO_ERROR;
        WinDRBDSvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        WinDRBDSvcStatus.dwCheckPoint = 0;
        WinDRBDSvcStatus.dwWaitHint = 0;

        if (!SetServiceStatus(WinDRBDSvcStatusHandle, &WinDRBDSvcStatus))
        {
            fprintf(stderr, "SetServiceStatus() failed");
        }

        return;
    }

    SetServiceStatus(WinDRBDSvcStatusHandle, &WinDRBDSvcStatus);
}

VOID
CALLBACK
WinDRBDSvcStart(DWORD a, LPSTR *b)
{
    WinDRBDSvcStatus.dwServiceType = SERVICE_WIN32;
    WinDRBDSvcStatus.dwCurrentState = SERVICE_START_PENDING;
    WinDRBDSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    WinDRBDSvcStatus.dwWin32ExitCode = NO_ERROR;
    WinDRBDSvcStatus.dwServiceSpecificExitCode = 0;
    WinDRBDSvcStatus.dwCheckPoint = 0;
    WinDRBDSvcStatus.dwWaitHint = 0;

    WinDRBDSvcStatusHandle = RegisterServiceCtrlHandler(WINDRBDUM_SVC,
        WinDRBDSvcCtrlHandler);
    if (WinDRBDSvcStatusHandle == (SERVICE_STATUS_HANDLE)0)
    {
        fprintf(stderr, "RegisterServiceCtrlHandler() failed");
        return;
    }

    WinDRBDSvcStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(WinDRBDSvcStatusHandle, &WinDRBDSvcStatus);

    for (;;)
    {
        if (WaitForSingleObject(WinDRBDSvcStopEvent, 0) != WAIT_TIMEOUT)
        {
            WinDRBDSvcStatus.dwWin32ExitCode = NO_ERROR;
            break;
        }
		/* do something */
    }

    WinDRBDSvcStatus.dwCurrentState = SERVICE_STOPPED;
    WinDRBDSvcStatus.dwControlsAccepted = 0;
    SetServiceStatus(WinDRBDSvcStatusHandle, &WinDRBDSvcStatus);
}

int
CALLBACK
WinMain(HINSTANCE a,
    HINSTANCE b,
    LPSTR c,
    int d)
{
    SERVICE_TABLE_ENTRY ServiceTable[] =
    {
        { WINDRBDUM_SVC, WinDRBDSvcStart },
        { NULL, NULL }
    };

    WinDRBDSvcStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (WinDRBDSvcStopEvent == NULL)
    {
        fprintf(stderr, "CreateEvent() failed");
        return 0;
    }

    if (!StartServiceCtrlDispatcher(ServiceTable))
    {
        MessageBoxA(NULL, "This program can only run as a Windows NT service.",
            "WinDRBD Service",
            MB_ICONSTOP | MB_TASKMODAL);

        return 0;
    }

    SetEvent(WinDRBDSvcStopEvent);
    
    return 1;
}

