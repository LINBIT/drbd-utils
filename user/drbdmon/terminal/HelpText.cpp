#include <terminal/HelpText.h>
#include <terminal/DisplayId.h>

namespace helptext
{
    // 32 kiB (0x8000) default space for help text
    // Larger help text size will still work, but less efficiently
    const size_t DFLT_HELP_TEXT_SPACE    = 0x8000;

    const char* const RSC_LINE_HELP =
        "  Markers:\n"
        "    Selection marker\n"
        "      (Unicode: filled in square, ASCII: asterisk)\n"
        "    Warning/Alert marker\n"
        "      (Unicode: yellow hollow/red filled in triangle, ASCII: yellow/red exclamation mark)\n"
        "    Role symbol:\n"
        "      Primary (Unicode: filled in circle, ASCII: plus sign)\n"
        "      Secondary (Unicode: hollow circle, ASCII: minus sign)\n"
        "      Unknown resource role (Question mark)\n"
        "  Resource name\n"
        "  Volume status:\n"
        "    Volume alert marker\n"
        "    Count of volumes with a normal state\n"
        "    Total count of volumes\n"
        "  Connection status:\n"
        "    Connection alert marker\n"
        "    Count of connections with a normal state\n"
        "    Total count of connections\n"
        "  Quorum indicator (Y or N)\n\n";

    const char* const RSC_LIST_HELP_1 =
        "\x1B\x01" "Help - Resource list" "\x1B\xFF" "\n"
        "\n"
        "The resource list display shows an overview of all DRBD resources that are currently "
        "active on the local node.\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Resource list overview\n"
        "Display columns and symbols\n"
        "Navigation Keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Resource list overview" "\x1B\xFF" "\n"
        "\n"
        "The resource list display show the list of active DRBD resources and a summary of the state of "
        "its volumes and connections.\n"
        "\n"
        "\x1B\x01" "Display columns and symbols" "\x1B\xFF" "\n"
        "\n"
        "The resource list display shows, from left to right, the following columns:\n"
        "\n";

    const char* const RSC_LIST_HELP_2 =
        "\x1B\x05" "ENTER      " "\x1B\xFF" " Show resource details for highlighted resource\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows resource actions menu\n"
        "\x1B\x05" "C          " "\x1B\xFF" " Shows connections list\n"
        "\x1B\x05" "R          " "\x1B\xFF" " Shows resource list\n"
        "\x1B\x05" "V          " "\x1B\xFF" " Shows volumes list\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the following commands are available "
        "within the resource list display. The commands work either on the currently highlighted resource or else "
        "a selection of one or more resources.\n";

    const char* const RSC_LIST_HELP_3 =
        "\x1B\x04" "connect" "\x1B\xFF" "\n"
        "    Connect the local node to the resource\n"
        "\x1B\x04" "connect-discard" "\x1B\xFF" "\n"
        "    Same as connect, but forces the local node to discard its data modifications for the resource\n"
        "\x1B\x04" "disconnect" "\x1B\xFF" "\n"
        "    Disconnect the local node from the resource\n"
        "\x1B\x04" "invalidate" "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF Invalidates local node's data for the resource, "
        "forcing a resynchronization if connected\n";

    const char* const VLM_LINE_HELP =
        "  Markers:\n"
        "    Selection marker\n"
        "      (Unicode: filled in square, ASCII: asterisk)\n"
        "    Warning/Alert marker\n"
        "      (Unicode: yellow hollow/red filled in triangle, ASCII: yellow/red exclamation mark)\n"
        "    DRBD volume number\n"
        "    Minor number of the block special file associated with the DRBD volume\n"
        "    Disk state of the DRBD volume\n"
        "    Quorum indicator (Y or N)\n"
        "    Resynchronization progress, if a resynchronization is currently in progress\n\n";

    const char* const VLM_LIST_HELP_1 =
        "\x1B\x01" "Help - Volume list" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume list overview\n"
        "Display columns and symbols\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Volume list overview" "\x1B\xFF" "\n"
        "\n"
        "The volume list display shows a list of all active volumes for a resource and a summary of "
        "details about each volume on the local node. Typically, you access this display from the resource "
        "details display. The summarized details shown for each volume include: volume "
        "number, DRBD minor number, disk state, quorum status, and synchroniziation "
        "percentage if a synchronization is in progress.\n"
        "\n"
        "\x1B\x01" "Display columns and symbols" "\x1B\xFF" "\n"
        "\n"
        "The line that shows the resource that the volume list belongs to shows, from "
        "left to right, the following columns:\n";

    const char* const VLM_LIST_HELP_2 =
        "The list of volumes shows, from left to right, the following columns:\n";

    const char* const VLM_LIST_HELP_3 =
        "\x1B\x05" "ENTER      " "\x1B\xFF" " Show volume details for highlighted resource\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows volume actions menu.\n"
        "            This key also works in page mode if you have a currently selected volume.\n"
        "\x1B\x05" "C          " "\x1B\xFF" " Shows connections list\n"
        "\x1B\x05" "R          " "\x1B\xFF" " Shows resource list\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the"
        "following DRBD commands are available within the volume list display. The"
        "commands work either on the currently highlighted volume or else a selection of"
        "one or more volumes.\n";

    const char* const PEER_VLM_LIST_HELP_1 =
        "\x1B\x01" "Help - Peer volume list" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume list overview\n"
        "Display columns and symbols\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Peer volume list overview" "\x1B\xFF" "\n"
        "\n"
        "The peer volume list display shows a list of all active volumes for a resource on a peer node "
        "and a summary of details about each volume on the local node. "
        "Typically, you access this display from the connection details display.\n"
        "\n"
        "\x1B\x01" "Display columns and symbols" "\x1B\xFF" "\n\n"
        "The line that shows the resource that the peer volume list belongs to shows, from "
        "left to right, the following columns:\n";

    const char* const PEER_VLM_LIST_HELP_2 =
        "The line that shows the connection that the peer volume list belongs to shows, from "
        "left to right, the following columns:\n";

    const char* const PEER_VLM_LIST_HELP_3 =
        "The list of peer volumes shows, from left to right, the following columns:\n"
        "\n"
        "  Markers:\n"
        "    Selection marker\n"
        "      (Unicode: filled in square, ASCII: asterisk)\n"
        "    Warning/Alert marker\n"
        "      (Unicode: yellow hollow/red filled in triangle, ASCII: yellow/red exclamation mark)\n"
        "  DRBD volume number\n"
        "  Minor number of the block special file associated with the DRBD volume\n"
        "  Disk state of the DRBD volume\n"
        "  Quorum indicator\n"
        "  Resynchronization progress, if a resynchronization is currently in progress\n\n";

    const char* const PEER_VLM_LIST_HELP_4 =
        "\x1B\x05" "ENTER      " "\x1B\xFF" " Show volume details for highlighted resource\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows volume actions menu.\n"
        "                This key also works in page mode if you have a currently selected volume.\n"
        "\x1B\x05" "C          " "\x1B\xFF" " Shows connections list\n"
        "\x1B\x05" "R          " "\x1B\xFF" " Shows resource list\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the "
        "following DRBD commands are available within the peer volume list display. The "
        "commands work either on the resource or the connection associated with the peer volumes.\n";

    const char* const PEER_VLM_DETAIL_HELP_1 =
        "\x1B\x01" "Help - Peer volume details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume details overview\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x03" "Experimental version - Help text not available yet." "\x1B\xFF" "\n";

    const char* const PEER_VLM_ACT_HELP_1 =
        "\x1B\x01" "Help - Peer volume details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume actions overview\n"
        "Volume actions\n"
        "\n"
        "\n"
        "\x1B\x03" "Experimental version - Help text not available yet." "\x1B\xFF" "\n";

    const char* const CON_LIST_HELP_1 =
        "\x1B\x01" "Help - Connection list" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Connection list overview\n"
        "Display columns and symbols\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Connection list overview" "\x1B\xFF" "\n"
        "\n"
        "The connection list display shows a list of peer node connections for a resource "
        "and a summary of details about each connection. Typically, you access this "
        "display from the resource details display.\n"
        "\n"
        "\x1B\x01" "Display columns and symbols" "\x1B\xFF" "\n"
        "\n"
        "The line that shows the resource that the connection list belongs to shows, from "
        "left to right, the following columns:\n";

    const char* const CON_LIST_HELP_2 =
        "The list of connections shows, from left to right, the following columns:\n";

    const char* const CON_LIST_HELP_3 =
        "\x1B\x05" "ENTER      " "\x1B\xFF" " Show connection details for highlighted connection\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows connection actions menu.\n"
        "            This key also works in page mode if you have a currently selected connection."
        "\x1B\x05" "R          " "\x1B\xFF" " Shows resource list\n"
        "\x1B\x05" "V          " "\x1B\xFF" " Shows volume list\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the "
        "following DRBD commands are available within the connection list display. The "
        "commands work either on the currently highlighted connection or else a selection of "
        "one or more connections.\n";

    const char* const CON_LINE_HELP =
        "  Markers:\n"
        "    Selection marker\n"
        "      (Unicode: filled in square, ASCII: asterisk)\n"
        "    Warning/Alert marker\n"
        "      (Unicode: yellow hollow/red filled in triangle, ASCII: yellow/red exclamation mark)\n"
        "    Role symbol:\n"
        "      Primary (Unicode: filled in circle, ASCII: plus sign)\n"
        "      Secondary (Unicode: hollow circle, ASCII: minus sign)\n"
        "      Unknown resource role (Question mark)\n"
        "  Peer node name\n"
        "  Volume status:\n"
        "    Volume alert marker\n"
        "    Count of volumes with a normal state\n"
        "    Total count of volumes\n"
        "  Connection status\n"
        "  Quorum indicator (Y or N)\n\n";

    const char* const TASKQ_HELP_1 =
        "\x1B\x01" "Help - Task queue" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Types of task queues\n"
        "Navigation keys\n"
        "\n"
        "\n"
        "\x1B\x01" "Types of task queues" "\x1B\xFF" "\n"
        "\n"
        "The task queue displays show the list of entries of the respective task queue. There are four "
        "task queues:\n"
        "\x1B\x01" "    Active tasks queue (ACTQ)" "\x1B\xFF" "\n"
        "        Shows tasks that are currently running\n"
        "\x1B\x01" "    Pending tasks queue (PNDQ)" "\x1B\xFF" "\n"
        "        Shows tasks that are pending execution. Tasks in this queue move to the active queue automatically "
        "as active tasks complete, or they can be manually moved to the suspended tasks queue instead\n"
        "\x1B\x01" "    Suspended tasks queue (SSPQ)" "\x1B\xFF" "\n"
        "        Shows tasks that are suspended. Tasks in this queue may be canceled or may be moved to the "
        "pending tasks queue.\n"
        "\x1B\x01" "    Finished tasks queue (FINQ)" "\x1B\xFF" "\n"
        "        Shows tasks that have finished execution. A task automatically moves from the active tasks queue "
        "to the finished tasks queue as the task completes.\n"
        "\n";

    const char* const TASKQ_HELP_2 =
        "For the pending tasks queue:\n"
        "\x1B\x05" "    S          " "\x1B\xFF" " Moves the task to the suspended tasks queue\n"
        "\x1B\x05" "    BACKSPACE  " "\x1B\xFF" " Same as " "\x1B\x05" "S" "\x1B\xFF" "\n"
        "For the suspended tasks queue:\n"
        "\x1B\x05" "    P          " "\x1B\xFF" " Moves the task to the pending tasks queue\n"
        "\x1B\x05" "    INSERT     " "\x1B\xFF" " Same as " "\x1B\x05" "P" "\x1B\xFF" "\n"
        "For the pending, suspended and finished tasks queue:\n"
        "\x1B\x05" "    DEL        " "\x1B\xFF" " Deletes the task entry\n"
        "For the active tasks queue:\n"
        "\x1B\x05" "    DEL        " "\x1B\xFF" " Terminates the running task\n";

    const char* const TASK_DETAIL_HELP_1 =
        "Help - Task details\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Task details overview\n"
        "\n"
        "\n"
        "\x1B\x01" "Task details overview" "\x1B\xFF" "\n"
        "\n"
        "The task detail display shows information about a task entry on one of the task queues.\n"
        "\n"
        "The information provided includes the command line being executed, and, depending on the task's state, "
        "may include the ID of the operating system process running the task, the exit code of a completed task "
        "and the data that a task has written to the standard output and/or standard error streams. The amount "
        "of process output that is saved for reviewing in DRBDmon is limited, therefore, the output data that is "
        "shown in the task details display may be truncated.\n"
        "\n";

    const char* const TASK_DETAIL_HELP_2 =
        "\x1B\x05" "S          " "\x1B\xFF" " Moves a pending task to the suspended tasks queue\n"
        "\x1B\x05" "BACKSPACE  " "\x1B\xFF" " Same as " "\x1B\x05" "S" "\x1B\xFF" "\n"
        "\x1B\x05" "P          " "\x1B\xFF" " Moves a suspended task to the pending tasks queue\n"
        "\x1B\x05" "INSERT     " "\x1B\xFF" " Same as " "\x1B\x05" "P" "\x1B\xFF" "\n"
        "\x1B\x05" "DEL        " "\x1B\xFF" " Terminates a running task\n";

    const char* const RSC_DETAIL_HELP_1 =
        "\x1B\x01" "Help - Resource details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Resource details overview\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Resource details overview" "\x1B\xFF" "\n"
        "\n"
        "The Resource details display shows information about a resource. Typically, you "
        "access this display from the resource list page while in cursor mode, after "
        "pressing ENTER on the currently highlighted resource.\n"
        "The resource information shown is similar to what is shown in the resource list "
        "display but in an unabbreviated way.\n"
        "\n"
        "\x1B\x01" "Navigation Keys" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows the resource actions menu for the resource.\n"
        "            See \"Help - Resource actions\" for more details.\n"
        "\x1B\x05" "C          " "\x1B\xFF" " Shows the connection list for the resource.\n"
        "\x1B\x05" "V          " "\x1B\xFF" " Shows the volume list for the resource.\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the "
        "following DRBD commands are available within the resource details display.\n";

    const char* const VLM_DETAIL_HELP_1 =
        "\x1B\x01" "Help - Volume details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume details overview\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Volume details overview" "\x1B\xFF" "\n"
        "\n"
        "The volume details display shows information about a resource's volume."
        "Typically, you access this display from the volume list display while in cursor "
        "mode, after pressing ENTER on the currently highlighted volume."
        "The volume information shown is similar to what is shown in the volume list "
        "display but in an unabbreviated way.\n"
        "\n"
        "\x1B\x01" "Navigation Keys" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows the volume actions menu for the volume.\n"
        "            See \"Help - Volume actions\" for more details.\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the "
        "following DRBD commands are available within the volume details display.\n";

    const char* const CON_DETAIL_HELP_1 =
        "\x1B\x01" "Help - Connection details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Connection details overview\n"
        "Navigation keys\n"
        "Commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Connection details overview" "\x1B\xFF" "\n"
        "\n"
        "The connection details display shows information about a resource's connection "
        "from the local node to a peer node. Typically, you access this display from the "
        "connection list display while in cursor mode, after pressing ENTER on the "
        "currently highlighted connection. "
        "The connection information shown is similar to what is shown in the connection "
        "list display but in an unabbreviated way.\n"
        "\n"
        "\x1B\x01" "Navigation Keys" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x05" "A          " "\x1B\xFF" " Shows the connection actions menu for the connection\n"
        "            See \"Help - Connection actions\" for more details.\n"
        "\x1B\x05" "V          " "\x1B\xFF" " Shows the peer volume list\n"
        "\n"
        "\x1B\x01" "Commands" "\x1B\xFF" "\n"
        "\n"
        "In addition to DRBDmon commands (see help index -> DRBDmon commands), the "
        "following DRBD commands are available within the connection details display.\n";

    const char* const RSC_ACT_HELP_1 =
        "\x1B\x01" "Help - Resource actions" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Resource actions overview\n"
        "Resource actions\n"
        "\n"
        "\n"
        "\x1B\x01" "Resource actions overview" "\x1B\xFF" "\n"
        "\n"
        "The resource actions display shows a menu of actions that you can choose from to "
        "affect the selected resource. Typically, you access this display from the "
        "resource details display. Actions correspond to various drbdadm commands.\n"
        "\n"
        "\x1B\x01" "Resource actions" "\x1B\xFF" "\n"
        "\n"
        "The resource actions available are:\n"
        "\x1B\x04" "Start resource" "\x1B\xFF" "\n"
        "    Start the resource on local node\n"
        "\x1B\x04" "Stop resource" "\x1B\xFF" "\n"
        "    Stop the resource on local node\n"
        "\x1B\x04" "Adjust resource" "\x1B\xFF" "\n"
        "    Adjust the resource on local node, for example, after a DRBD resource configuration change\n"
        "\x1B\x04" "Make primary" "\x1B\xFF" "\n"
        "    Make the local node primary for the resource\n"
        "\x1B\x04" "Make secondary" "\x1B\xFF" "\n"
        "    Make the local node secondary for the resource\n"
        "\x1B\x04" "Connect" "\x1B\xFF" "\n"
        "    Connect the local node to the resource\n"
        "\x1B\x04" "Disconnect" "\x1B\xFF" "\n"
        "    Disconnect the local node from the resource\n"
        "\x1B\x04" "Run verification" "\x1B\xFF" "\n"
        "    Verifies that the data on this node matches the data on a peer node\n"
        "\x1B\x04" "Pause resynchronization" "\x1B\xFF" "\n"
        "    Pauses local node resource synchronization with peer nodes\n"
        "\x1B\x04" "Resume resynchronization" "\x1B\xFF" "\n"
        "    Resumes local node resource synchronization with peer nodes\n"
        "\x1B\x04" "Force make primary" "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF This may result in overwriting the data on peer volumes\n"
        "\x1B\x04" "Discard & resolve split-brain" "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF Same as connect but forces local node to discard its data modifications "
        "for the resource\n"
        "\x1B\x04" "Invalidate & resynchronize" "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF Invalidates local node's data for the resource, forcing a "
        "resynchronization\n";

    const char* const VLM_ACT_HELP_1 =
        "\x1B\x01" "Help - Volume actions" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Volume actions overview\n"
        "Volume actions\n"
        "\n"
        "\n"
        "\x1B\x01" "Volume actions overview" "\x1B\xFF" "\n"
        "\n"
        "The volume actions display shows a menu of actions that you can choose from to "
        "affect the selected volume for a given resource. Typically, you access this "
        "display from the volume details display. Actions correspond to various drbdadm "
        "and drbdsetup commands.\n"
        "\n"
        "\x1B\x01" "Volume actions" "\x1B\xFF" "\n"
        "\n"
        "The volume actions available are:\n"
        "\x1B\x04" "Attach       " "\x1B\xFF" "\n"
        "    Attach the local backing storage device\n"
        "\x1B\x04" "Detach       " "\x1B\xFF" "\n"
        "    Detach the local backing storage device\n"
        "\x1B\x04" "Resize       " "\x1B\xFF" "\n"
        "    Resizes the DRBD volume after a change of the size of its backing storage\n"
        "\x1B\x04" "Invalidate   " "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF Invalidates the local node's data for the volume, "
        "forcing a resynchronization if connected\n";

    const char* const CON_ACT_HELP_1 =
        "\x1B\x01" "Help - Connection actions" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Connection actions overview\n"
        "Connection actions\n"
        "\n"
        "\n"
        "\x1B\x01" "Connection actions overview" "\x1B\xFF" "\n"
        "\n"
        "The connection actions display shows a menu of actions that you can choose from "
        "to affect the selected connection between the local node and a peer node for a "
        "given resource. Typically, you access this display from the connection details "
        "display. Actions correspond to various drbdadm or drbdsetup commands.\n"
        "\n"
        "\x1B\x01" "Connection actions" "\x1B\xFF" "\n"
        "\n"
        "The connection actions available are:\n"
        "\x1B\x04" "Connect" "\x1B\xFF" "\n"
        "    Connects to the peer node\n"
        "\x1B\x04" "Disconnect" "\x1B\xFF" "\n"
        "    Disconnects from the peer node\n"
        "\x1B\x04" "Connect & resolve split-brain" "\x1B\xFF" "\n"
        "    Same as connect, but forces the local node to discard its data modifications for the resource. "
        "This command is normally used to discard changes that are the result of a split brain situation.\n";

    const char* const MSG_LOG_HELP_1 =
        "\x1B\x01" "Help - Message log" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Message log overview\n"
        "Navigation keys\n"
        "\n"
        "\n"
        "\x1B\x01" "Message log overview" "\x1B\xFF" "\n"
        "\n"
        "The message log display shows a list of DRBD log events and a summary of "
        "details about each event. Typically, you access this display from the main menu "
        "display. The summarized details shown for each log event include: the severity "
        "level of the event, the time of the time, and the first line of the event's log message. "
        "The first line of the log message will be truncated if it is too long to fit on one line. "
        "The full log message can be displayed using the message details display.\n"
        "\n";

    const char* const MSG_LOG_HELP_2 =
        "\n"
        "In cursor navigation mode:\n"
        "\x1B\x05" "ARROW UP   " "\x1B\xFF" " Move cursor to next log event above cursor\n"
        "\x1B\x05" "ARROW DOWN " "\x1B\xFF" " Move cursor to next log event below cursor\n"
        "\x1B\x05" "ENTER      " "\x1B\xFF" " Show message details for highlighted log event\n"
        "\x1B\x05" "SPACE      " "\x1B\xFF" " Toggle log event selection\n"
        "\x1B\x05" "F4         " "\x1B\xFF" " Clear selection\n"
        "\x1B\x05" "DEL        " "\x1B\xFF" " Delete highlighted entry or all selected entries\n";

    const char* const MSG_DETAIL_HELP_1 =
        "\x1B\x01" "Help - Message details" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Message details overview\n"
        "Navigation keys\n"
        "\n"
        "\n"
        "\x1B\x01" "Message details overview" "\x1B\xFF" "\n"
        "\n"
        "The message details display shows the alert level, date, time and message of a log entry. Messages "
        "that consist of multiple lines of text and messages that are too long for display in the list shown "
        "by the message log display can be viewed in full in the message details view.\n"
        "\n";

    const char* const CONF_HELP_1 =
        "\x1B\x01" "Help - Configuration" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Configuration settings\n"
        "Changing the configuration from the command line\n"
        "\n"
        "\n"
        "\x1B\x01" "Configuration settings" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x04" "Mouse navigation" "\x1B\xFF" "\n"
        "    Enables navigation by mouse. If disabled, in most terminals, the mouse can instead be used "
        "to mark displayed text for copying. If enabled, the displays react to clicks in various areas.\n"
        "\x1B\04" "Display interval" "\x1B\xFF" "\n"
        "    Set the interval between display updates caused by changes of the DRBD state. This can be useful "
        "if DRBDmon is displayed remotely, for example, through an SSH connection with limited bandwidth or "
        "high latency.\n"
        "\x1B\x04" "Discard successfully completed tasks" "\x1B\xFF" "\n"
        "    Instead of moving task entries of completed tasks that ended with an exit code of 0 to the "
        "finished tasks queue, discards those task entries\n"
        "\x1B\x04" "Discard all completed tasks" "\x1B\xFF" "\n"
        "    Discards task entries upon completion of the task, instead of moving them to the finished tasks queue\n"
        "\x1B\x04" "Suspend new tasks" "\x1B\xFF" "\n"
        "    Instead starting new tasks immediately or moving the associated task entries into the pending tasks "
        "queue, places new task entries into the suspended tasks queue, from where they can either be manually moved "
        "to the pending tasks queue, or be manually discarded\n"
        "\x1B\x04" "Color scheme" "\x1B\xFF" "\n"
        "    Sets the display color scheme\n"
        "\x1B\x04" "Character set" "\x1B\xFF" "\n"
        "    Sets the display character set\n"
        "\n"
        "\x1B\x01" "Changing the configuration from the command line" "\x1B\xFF" "\n"
        "\n"
        "The persistent DRBDmon configuration can be displayed and changed using command line arguments.\n"
        "\n"
        "The following configuration commands are available:\n"
        "\x1B\x04" "settings" "\x1B\xFF" "\n"
        "    Displays the current configuration.\n"
        "    If a configuration file is present, the persistent configuration is loaded and displayed. If there is "
        "no configuration file or the configuration file is inaccessible, the default configuration is displayed.\n"
        "\x1B\x04" "get " "\x1B\x05" "key" "\x1B\xFF" "\n"
        "    Shows the value set for the specified " "\x1B\x05" "key" "\x1B\xFF" "\n"
        "\x1B\x04" "set " "\x1B\x05" "key value" "\x1B\xFF" "\n"
        "    Sets the specified " "\x1B\x05" "value" "\x1B\xFF" " on the entry identified by the specified "
        "\x1B\x05" "key" "\x1B\xFF" "\n"
        "\n"
        "If the " "\x1B\x04" "set" "\x1B\xFF" " command is used, but no configuration file exists, then a "
        "new configuration file is created.\n"
        "\n"
        "Deleting the configuration file effectively resets the configuration to the default values.\n";

    const char* const GENERAL_HELP_1 =
        "\x1B\x01" "General help" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Navigating DRBDmon displays\n"
        "Mouse navigation\n"
        "Using the DRBDmon command line\n"
        "\n"
        "\n"
        "\x1B\x01" "Navigating DRBDmon displays" "\x1B\xFF" "\n"
        "\n"
        "DRBDmon functions with the concept of a display stack, where navigating to a display, puts that display "
        "on top of DRBDmon's current display, and closing a display returns you to the previous display. Various "
        "hotkeys, menu items or items clickable by mouse on a display will open another display. Most displays are "
        "also directly accessible from the main menu, which can be opened from any display by pressing the "
        "\x1B\x05" "F2" "\x1B\xFF function key. Displays that can be navigated to directly also show a display ID "
        "in the upper left corner below the title bar, and this display ID can be used with the " "\x1B\x04"
        "/display" "\x1B\xFF command to open that display. A display is closed by pressing the \x1B\x05" "F10"
        "\x1B\xFF function key. Closing all displays takes you back to the main menu.\n"
        "\n"
        "Some displays, such as, for example, the various DRBD object lists or message lists, support a cursor "
        "navigation mode where a cursor can be used to highlight and track list items. In contrast to the usual "
        "page navigation mode, the display will automatically switch to the page that contains the item under the "
        "cursor in cursor navigation mode.\n"
        "\n"
        "\x1B\x01" "Mouse navigation" "\x1B\xFF" "\n"
        "\n"
        "If mouse navigation is enabled in the DRBDmon configuration, and provided that the terminal that is used "
        "to run DRBDmon supports mouse reporting, various items in DRBDmon can be clicked by using the mouse to "
        "navigate or to perform actions. In most terminals, mouse navigation prevents the selection of text as a "
        "side effect. Mouse navigation can be turned off temporarily in the DRBDmon configuration for selecting "
        "and copying text displayed by DRBDmon.\n"
        "\n"
        "\x1B\x01" "Using the DRBDmon command line" "\x1B\xFF" "\n"
        "\n"
        "Typing the slash (" "\x1B\x05" "/" "\x1B\xFF" ") character places the cursor on the command line. "
        "DRBDmon commands and DRBD commands can be entered on the command line. Entering a space character after "
        "the first word on the command line triggers command completion. If there is a unique match, the matching "
        "command will be inserted into the command line. If there are multiple possible matches, a partial "
        "command completion is attempted. If the command line starts with a single slash, DRBDmon commands will "
        "override DRBD commands during command completion, meaning that if there is a single matching DRBDmon "
        "command, but also one or more matching DRBD commands, the matching DRBDmon command will be inserted into "
        "the command line. DRBD commands can be entered preceded by only a single slash for convenience. "
        "Using two slashes limits the command completion to match only DRBD commands.\n"
        "Regardless of any other possible matches, " "\x1B\x04" "/d" "\x1B\xFF is always interpreted as the "
        "\x1B\x04" "/display" "\x1B\xFF command by the command completion function.\n"
        "Command line entry can be canceled by pressing the " "\x1B\x05" "F12" "\x1B\xFF "
        "function key, or by deleting all contents from the command line.\n";

    const char* const INSERT_NAV_HELP_1 =
        "\x1B\x01" "Navigation Keys" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x05" "PAGE UP    " "\x1B\xFF" " Show previous page, if multiple pages\n"
        "\x1B\x05" "PAGE DOWN  " "\x1B\xFF" " Show next page, if multiple pages\n"
        "\x1B\x05" "F1         " "\x1B\xFF" " Show help text\n"
        "\x1B\x05" "F3         " "\x1B\xFF" " Exit DRBDmon\n"
        "\x1B\x05" "F5         " "\x1B\xFF" " Refresh display\n"
        "\x1B\x05" "F6         " "\x1B\xFF" " Go to page number entry field\n"
        "            \x1B\x05" "ENTER" "\x1B\xFF" " or " "\x1B\x05" "TAB" "\x1B\xFF" " confirm page number entry\n"
        "            \x1B\x05" "F12" "\x1B\xFF" " cancels page number entry\n"
        "\x1B\x05" "F10        " "\x1B\xFF" " Close display\n"
        "\x1B\x05" "/          " "\x1B\xFF" " Go to command line entry field\n"
        "            \x1B\x05" "F12" "\x1B\xFF" " cancels command line entry\n";

    const char* const INSERT_NAV_LIST_HELP_1 =
        "\x1B\x05" "TAB        " "\x1B\xFF" " Toggle navigation mode between Page and Cursor\n"
        "\x1B\x05" "F4         " "\x1B\xFF" " Clear selection\n"
        "\n"
        "In cursor navigation mode:\n"
        "\x1B\x05" "SPACE      " "\x1B\xFF" " Toggle highlighted item selection\n"
        "\x1B\x05" "ARROW UP   " "\x1B\xFF" " Move cursor to next item above cursor\n"
        "\x1B\x05" "ARROW DOWN " "\x1B\xFF" " Move cursor to next item below cursor\n";

    const char* const INSERT_RSC_CMD_HELP_1 =
        "\x1B\x04" "//start        " "\x1B\xFF" "\n"
        "    Start the resource on local node\n"
        "\x1B\x04" "//stop         " "\x1B\xFF" "\n"
        "    Stop the resource on local node\n"
        "\x1B\x04" "//adjust       " "\x1B\xFF" "\n"
        "    Adjust the resource on local node, for example, after a DRBD resource configuration change\n"
        "\x1B\x04" "//primary      " "\x1B\xFF" "\n"
        "    Make the local node primary for the resource\n"
        "\x1B\x04" "//secondary    " "\x1B\xFF" "\n"
        "    Make the local node secondary for the resource\n";

    const char* const INSERT_VLM_CMD_HELP_1 =
        "\x1B\x04" "//attach       " "\x1B\xFF" "\n"
        "    Attach the local backing storage device\n"
        "\x1B\x04" "//detach       " "\x1B\xFF" "\n"
        "    Detach the local backing storage device\n"
        "\x1B\x04" "//invalidate   " "\x1B\xFF" "\n"
        "    \x1B\x03" "CAUTION:" "\x1B\xFF Invalidates the local node's data for the volume, "
        "forcing a resynchronization if connected\n";

    const char* const INSERT_CON_CMD_HELP_1 =
        "\x1B\x04" "//connect" "\x1B\xFF" "\n"
        "    Connects to the peer node\n"
        "\x1B\x04" "//connect-discard" "\x1B\xFF" "\n"
        "    Same as connect, but forces the local node to discard its data modifications for the resource. "
        "This command is normally used to discard changes that are the result of a split brain situation.\n"
        "\x1B\x04" "//disconnect" "\x1B\xFF" "\n"
        "    Disconnects from the peer node\n";

    const char* const DRBDMON_CMD_HELP_1 =
        "\x1B\x01" "Help - DRBDmon commands" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Description of available commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Description of available commands" "\x1B\xFF" "\n"
        "\n"
        "The following DRBDmon commands are available:\n"
        "\x1B\x04" "/display" "\x1B\xFF" "\n"
        "    Switches to the display the identifier of which is specified in the argument to the command. "
        "The identifier of a display is shown below the title bar in the upper left corner of the "
        "respective display.\n"
        "    Example: /display main\n"
        "        Switches to the main menu display\n"
        "    The string " "\x1B\x04" "/d" "\x1B\xFF" " always completes to the " "\x1B\x04" "/display" "\x1B\xFF"
        " command for convenience.\n"
        "\x1B\x04" "/select-all" "\x1B\xFF" "\n"
        "    Selects all items of a list display. For example, running the /select-all command from the "
        "resource list display selects all resources. If only items with warnings or alerts are displayed due "
        "to a problem mode have been selected, only those items are selected.\n"
        "\x1B\x04" "/clear-selection" "\x1B\xFF" "\n"
        "    Clears the selection of items of a list display. For example, running the "
        "\x1B\x04" "/clear-selection" "\x1B\xFF"
        " command from the resource list display clears the selection of resources.\n"
        "\x1B\x04" "/colors" "\x1B\xFF" "\n"
        "    Temporarily sets the selected color scheme.\n"
        "    Available color schemes are:\n"
        "        DARK256\n"
        "            256 colors scheme for terminals with a dark background color\n"
        "        DARK16\n"
        "            16 colors scheme for terminals with a dark background color\n"
        "        LIGHT256\n"
        "            256 colors scheme for terminals with a light background color\n"
        "        LIGHT16\n"
        "            16 colors scheme for terminals with a light background color\n"
        "        DEFAULT\n"
        "            Default color scheme\n"
        "\x1B\x04" "/charset" "\x1B\xFF" "\n"
        "    Temporarily sets the selected color scheme.\n"
        "    Available color schemes are:\n"
        "        UNICODE\n"
        "            Unicode character set (UTF-8)\n"
        "        UTF-8\n"
        "            Same as " "\x1B\x04" "UNICODE" "\x1B\xFF" "\n"
        "        ASCII\n"
        "            ASCII character set\n"
        "        DEFAULT\n"
        "            Default character set\n"
        "\x1B\x04" "/exit" "\x1B\xFF" "\n"
        "    Exits DRBDmon\n";

    const char* const DRBD_CMD_HELP_1 =
        "\x1B\x01" "Help - DRBD commands" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Usage of DRBD commands\n"
        "Description of available commands\n"
        "\n"
        "\n"
        "\x1B\x01" "Usage of DRBD commands" "\x1B\xFF" "\n"
        "\n"
        "DRBD commands can be used in two ways:\n"
        "    - In the context of the currently highlighted or selected item(s)\n"
        "    - With an argument specifying the DRBD resource, connection or volume for the command\n"
        "If a command is entered without any arguments, it will affect the item under the cursor, or, if "
        "there is a selection of multiple items, the selected items. A suitable item must be selected for "
        "the command to be able to be executed.\n"
        "\n"
        "\x1B\x01" "Description of available commands" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x04" "//start" "\x1B\xFF" "\n"
        "    Starts a DRBD resource\n"
        "\x1B\x04" "//stop" "\x1B\xFF" "\n"
        "    Stops a DRBD resource\n"
        "\x1B\x04" "//up" "\x1B\xFF" "\n"
        "    Starts a DRBD resource (same as //start)\n"
        "\x1B\x04" "//down" "\x1B\xFF" "\n"
        "    Stops a DRBD resource (same as //stop)\n"
        "\x1B\x04" "//adjust" "\x1B\xFF" "\n"
        "    Adjusts a DRBD resource, in order to make its runtime state match the settings in its "
        "configuration file\n"
        "\x1B\x04" "//primary" "\x1B\xFF" "\n"
        "    Promotes a DRBD resource to the Primary role\n"
        "\x1B\x04" "//force-primary" "\x1B\xFF" "\n"
        "    Forces promotion of an inconsistent or outdated DRBD resource to the Primary role\n"
        "\x1B\x04" "//secondary" "\x1B\xFF" "\n"
        "    Demotes a DRBD resource to the Secondary role\n"
        "\x1B\x04" "//force-secondary" "\x1B\xFF" "\n"
        "    Forces demotion of a DRBD resource to the Secondary role\n"
        "\x1B\x04" "//connect" "\x1B\xFF" "\n"
        "    Connects a resource's replication link\n"
        "\x1B\x04" "//disconnect" "\x1B\xFF" "\n"
        "    Disconnects a resource's replication link\n"
        "\x1B\x04" "//force-disconnect" "\x1B\xFF" "\n"
        "    Forces disconnection of a resource's replication link\n"
        "\x1B\x04" "//attach" "\x1B\xFF" "\n"
        "    Attaches local backing storage to a DRBD volume\n"
        "\x1B\x04" "//detach" "\x1B\xFF" "\n"
        "    Detaches local backing storage from a DRBD volume\n"
        "\x1B\x04" "//connect-discard" "\x1B\xFF" "\n"
        "    Connects a resource's replication link and discard the local volumes' data changes, which will cause it "
        "to resynchronize with the volumes of a peer node's resource to resolve a split-brain situation\n"
        "\x1B\x04" "//verify" "\x1B\xFF" "\n"
        "    Starts a DRBD verification process to compare the data on the local volume to the data on the volume "
        "of a peer node's resource\n"
        "\x1B\x04" "//pause-sync" "\x1B\xFF" "\n"
        "    Pauses resynchronization of a volume\n"
        "\x1B\x04" "//resume-sync" "\x1B\xFF" "\n"
        "    Resumes resynchronization of a volume\n"
        "\x1B\x04" "//invalidate" "\x1B\xFF" "\n"
        "    Invalidates the data on a local DRBD volume, thereby causing a full resynchronization of the "
        "data using another node as the data source\n"
        "\x1B\x04" "//invalidate-remote" "\x1B\xFF" "\n"
        "    Invalidates the data on a DRBD peer volume, thereby causing a full resynchronization of the "
        "data on a remote node, using another node as the data source\n";


    const char* const MAIN_MENU_HELP_1 =
        "\x1B\x01" "Help - Main menu" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x01" "Contents" "\x1B\xFF" "\n"
        "\n"
        "Main menu overview\n"
        "Navigation keys\n"
        "\n"
        "\n"
        "\x1B\x01" "Main menu overview" "\x1B\xFF" "\n"
        "\n"
        "The main menu shows a selection of options that navigate to various displays or that perform a function. "
        "Items can be selected from the main menu by entering the item's identifier, which is displayed to the left "
        "of the item's title.\n"
        "\n"
        "\x1B\x01" "Navigation Keys" "\x1B\xFF" "\n"
        "\n"
        "\x1B\x05" "F1         " "\x1B\xFF" " Show help text\n"
        "\x1B\x05" "F3         " "\x1B\xFF" " Exit DRBDmon\n"
        "\x1B\x05" "F5         " "\x1B\xFF" " Refresh display\n"
        "\x1B\x05" "F10        " "\x1B\xFF" " Close display, if opened on top of another display\n"
        "\x1B\x05" "/          " "\x1B\xFF" " Go to command line entry field\n"
        "            \x1B\x05" "F12" "\x1B\xFF" " cancels command line entry\n";

    void open_help_page(const id_type help_id, const ComponentsHub& dsp_comp_hub)
    {
        std::string& help_text = dsp_comp_hub.dsp_shared->help_text;

        help_text.clear();
        help_text.reserve(DFLT_HELP_TEXT_SPACE);

        switch (help_id)
        {
            case id_type::RSC_LIST:
            {
                help_text = helptext::RSC_LIST_HELP_1;
                help_text += helptext::RSC_LINE_HELP;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::RSC_LIST_HELP_2;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                break;
            }
            case id_type::RSC_DETAIL:
            {
                help_text = helptext::RSC_DETAIL_HELP_1;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                break;
            }
            case id_type::RSC_ACTIONS:
            {
                help_text = helptext::RSC_ACT_HELP_1;
                break;
            }
            case id_type::VLM_LIST:
            {
                help_text = helptext::VLM_LIST_HELP_1;
                help_text += helptext::RSC_LINE_HELP;
                help_text += helptext::VLM_LIST_HELP_2;
                help_text += helptext::VLM_LINE_HELP;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::VLM_LIST_HELP_3;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                help_text += helptext::INSERT_VLM_CMD_HELP_1;
                break;
            }
            case id_type::VLM_DETAIL:
            {
                help_text = helptext::VLM_DETAIL_HELP_1;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                help_text += helptext::INSERT_VLM_CMD_HELP_1;
                break;
            }
            case id_type::VLM_ACTIONS:
            {
                help_text = helptext::VLM_ACT_HELP_1;
                break;
            }
            case id_type::CON_LIST:
            {
                help_text = helptext::CON_LIST_HELP_1;
                help_text += helptext::RSC_LINE_HELP;
                help_text += helptext::CON_LIST_HELP_2;
                help_text += helptext::CON_LINE_HELP;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::CON_LIST_HELP_3;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                help_text += helptext::INSERT_CON_CMD_HELP_1;
                break;
            }
            case id_type::CON_DETAIL:
            {
                help_text = helptext::CON_DETAIL_HELP_1;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                help_text += helptext::INSERT_CON_CMD_HELP_1;
                break;
            }
            case id_type::CON_ACTIONS:
            {
                help_text = helptext::CON_ACT_HELP_1;
                break;
            }
            case id_type::PVLM_LIST:
            {
                help_text = helptext::PEER_VLM_LIST_HELP_1;
                help_text += helptext::RSC_LINE_HELP;
                help_text += helptext::PEER_VLM_LIST_HELP_2;
                help_text += helptext::CON_LINE_HELP;
                help_text += helptext::PEER_VLM_LIST_HELP_3;
                help_text += helptext::VLM_LINE_HELP;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::PEER_VLM_LIST_HELP_4;
                help_text += helptext::INSERT_RSC_CMD_HELP_1;
                help_text += helptext::INSERT_CON_CMD_HELP_1;
                break;
            }
            case id_type::PVLM_DETAIL:
            {
                break;
            }
            case id_type::PVLM_ACTIONS:
            {
                break;
            }
            case id_type::MAIN_MENU:
            {
                help_text = MAIN_MENU_HELP_1;
                break;
            }
            case id_type::MESSAGE_LOG:
            {
                help_text = helptext::MSG_LOG_HELP_1;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::MSG_LOG_HELP_2;
                break;
            }
            case id_type::MESSAGE_DETAIL:
            {
                help_text = helptext::MSG_DETAIL_HELP_1;
                help_text += helptext::INSERT_NAV_HELP_1;
                break;
            }
            case id_type::TASK_QUEUE:
            {
                help_text = helptext::TASKQ_HELP_1;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::INSERT_NAV_LIST_HELP_1;
                help_text += helptext::TASKQ_HELP_2;
                break;
            }
            case id_type::TASK_DETAIL:
            {
                help_text = helptext::TASK_DETAIL_HELP_1;
                help_text += helptext::INSERT_NAV_HELP_1;
                help_text += helptext::TASK_DETAIL_HELP_2;
                break;
            }
            case id_type::DRBDMON_COMMANDS:
            {
                help_text = helptext::DRBDMON_CMD_HELP_1;
                break;
            }
            case id_type::DRBD_COMMANDS:
            {
                help_text = helptext::DRBD_CMD_HELP_1;
                break;
            }
            case id_type::GENERAL_HELP:
            {
                help_text = helptext::GENERAL_HELP_1;
                break;
            }
            case id_type::CONF_HELP:
            {
                help_text = helptext::CONF_HELP_1;
                break;
            }
            default:
            {
                break;
            }
        }

        dsp_comp_hub.dsp_shared->help_text_updated = true;
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::HELP_VIEWER);
    }
}
