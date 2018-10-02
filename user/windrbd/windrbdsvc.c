/*

The handling of the Windows Services API was taken from
ImDisk (http://www.ltr-data.se/opencode.html). Since I
consider this 'substanstial portion of the software' I
include following license statement:

Copyright (C) 2005-2015 Olof Lagerkvist.

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or
sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include <windows.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_ARGS 4
#define NUM_WINDRBD_PROCESSES 2

struct windrbd_process {
	pid_t pid;
	const char *cmdline[MAX_ARGS];
};

struct windrbd_process windrbd_processes[NUM_WINDRBD_PROCESSES] =
	{ { -1, {"windrbd", "user-mode-helper-daemon", NULL, } },
	  { -1, {"windrbd", "log-server", NULL /* fname */ , NULL } }
	};

void start_processes_if_not_running(void)
{
	/* while waitpid(..., NOHANG) != 0 */
	/* find pid in table and set pid = -1 */

	/* foreach p in table */
	/* if pid == -1 fork() { exec() / pid = retval of fork } */

	/* sleep(1) */
}

void terminate_processes(void)
{
	/* for each p in table */
	/* if pid != -1 kill(, SIGKILL) and waitpid(pid, HANG) */
}

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
	start_processes_if_not_running();

	sleep(1);
    }
    terminate_processes();

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

