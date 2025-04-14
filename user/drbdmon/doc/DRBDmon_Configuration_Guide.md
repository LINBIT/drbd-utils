# DRBDmon Configuration Guide
  
## Index  
1  [Configurable settings](#Configuration)<br>
  1.1 [Mouse navigation](#MouseNav)<br>
  1.2 [Display interval](#DisplayInterval)<br>
  1.3 [Task queue settings](#TaskQ)<br>
  1.4 [Color scheme](#ColorScheme)<br>
  1.5 [Character set](#CharSet)<br>
2 [CLI configuration commands](#CLICommands)<br>
3 [Configuration entry keys and values](#ConfEntries)<br>
  3.1 [Configuration entry keys](#ConfKeys)<br>
  3.2 [Color scheme identifiers](#ConfColorValues)<br>
  3.3 [Character set identifiers](#ConfCharSetValues)<br>
4 [Configuration file](#ConfFile)<br>

<a id="Configuration" name="Configuration"></a>
## 1 Configurable settings

<a id="MouseNav" name="MouseNav"></a>
### 1.1 Mouse navigation
On terminals that support mouse reporting, a mouse can be used to navigate and to perform various operations in DRBDmon, if mouse navigation is enabled in the configuration.

With mouse navigation enabled, many terminals do not support selecting text for copy & paste purposes. With most of those terminals, temporarily disabling mouse navigation can allow you to select, copy and paste text in DRBDmon.

If DRBDmon does not react to mouse actions, despite mouse navigation being enabled, the most likely reason is that the terminal does not support mouse reporting. In this case, no mouse events are sent to DRBDmon by the terminal, and the mouse navigation setting has no effect.

<a id="DisplayInterval" name="DisplayInterval"></a>
### 1.2 Display interval

When the display is refreshed due to state changes of DRBD resources, the display interval setting can be used to limit the display refresh frequency. This can help to avoid overloading slow remote connection, such as remote SSH sessions with limited bandwidth and/or high latency, with traffic caused by too many refreshes of the DRBDmon display within a short time frame.

The display interval value is specified in milliseconds. It has no effect on display refreshes caused by user actions, such as switching to another display or selecting items.

<a id="TaskQ" name="TaskQ"></a>
### 1.3 Task queue settings

Task queues are used when DRBDmon needs to run external commands, for example, when the user performs actions on DRBD objects, or when the user enters DRBD commands.

**Discard successfully completed tasks**

If this option is enabled, completed tasks where the external process ended with an exit code of 0 (zero) are discarded automatically, instead of being placed in the finished tasks queue. Disable this option if you want to check the command line or the output of successfully completed tasks. Note that disabling this option may be overridden by enabling the “Discard all completed tasks” option.

**Discard all completed tasks**

If this option is enabled, all completed tasks are discarded automatically, instead of being paced in the finished tasks queue.

**Suspend new tasks**

If this option is enabled, new tasks are not scheduled automatically, and are placed in the suspended tasks queue instead. The user will have to move such tasks to the pending tasks queue manually to schedule the tasks.

This option can be used to check the command line of tasks, either before running a task, or to decide whether to run or to discard a task.

<a id="ColorScheme" name="ColorScheme"></a>
### 1.4 Color scheme

**Support of 256 color schemes**

The 256 colors schemes require support for the 8 bit / 256 color ANSI escape codes by the terminal.

As of 2024, this color mode is known to be supported by the following terminals:

* Konsole (KDE)
* Gnome terminal
* xterm
* Terminator
* macOS Terminal application
* iTerm
* Windows 10 PowerShell
* Windows 10 CMD shell

**Default**

If the default color scheme is selected, one of the other selectable color schemes will serve as the default color set, depending on the DRBDmon version.

**256 colors on dark background**

This color scheme is intended for use on terminals with a dark background color, such as black or dark gray.

As of DRBDmon V1R2M4, this is currently used as the default color scheme.

**16 colors on dark background**

This color scheme is intended for use on terminals with a dark background color, such as black or dark gray, if the terminal does not support the 256 colors mode.

#### 256 colors on light background

This color scheme is intended for use on terminals with a light background color, such as white, light gray or pale yellow.

#### 16 colors on light background

This color scheme is intended for use on terminals with a light background color, such as white, light gray or pale yellow, if the terminal does not support the 256 colors mode.

<a id="CharSet" name="CharSet"></a>
### 1.5 Character set

**Default**

If the default character set is selected, one of the other selectable character sets will serve as the default character set, depending on the DRBDmon version.

**Unicode (UTF-8)**

If the Unicode character set is selected, Unicode sequences will be used to display various symbols, such as, for example, the selection indicator, resource role indicator, and the warning and error indicators. Using this character set requires a terminal that supports UTF-8 unicode, and also requires the use of a display font by the terminal that provides the symbols used by DRBDmon.

**ASCII**

If the ASCII character set is selected, various symbols, such as, for example, the selection indicator, resource role indicator, and the warning and error indicators, will be displayed using ASCII characters only. This character set is suitable for use on terminals that do not support Unicode sequences, or for use with display fonts that do not provide suitable symbols for the set of Unicode symbols used by DRBDmon.

<a id="CLICommands" name="CLICommands"></a>
## 2 CLI configuration commands

DRBDmon configuration settings can be changed from the command line. The CLI configuration commands are:

**settings**

The settings command displays the current configuration.

If a configuration file is present, the persistent configuration is loaded and displayed. If there is no configuration file or the configuration file is inaccessible, the default configuration is displayed.

**get** *key*

The get command displays the configuration entry indentified by the specified key. If no configuration file exists, the default configuration entry is displayed.

**set** *key* *value*

The set command sets the specified value on the configuration entry identified by the specified key.

If there is no configuration file, a new configuration file is created.

<a id="ConfEntries" name="ConfEntries"></a>
## 3 Configuration entry keys and values

<a id="ConfKeys" name="ConfKeys"></a>
### 3.1 Configuration entry keys

| Configuration entry | Key identifier  | Value type | Default value
|--|--|--|--|
| Character set | Character set | UnsgInt16 | 0
| Color scheme | ColorScheme | UnsgInt16 | 0
| Discard successfully completed tasks | DiscardOkTasks | Boolean | true
| Discard all completed tasks | DiscardFailedTasks | Boolean | false
| Display interval | DisplayInterval | UnsgInt16 | 200
| Mouse navigation | EnableMouseNav | Boolean | true
| Suspend new tasks | SuspendNewTasks | Boolean | false

<a id="ConfColorValues" name="ConfColorValues"></a>
### 3.2 Color scheme identifier values
| Color scheme name | Identifier value
|--|--|
| Default | 0
| 256 colors on dark background | 1
| 16 colors on dark background | 2
| 256 colors on light background | 3
| 16 colors on light background | 4

<a id="ConfCharSetValues" name="ConfCharSetValues"></a>
### 3.3 Character set identifier values

| Character set name | Identifier value
|--|--|
| Default | 0
| Unicode (UTF-8) | 1
| ASCII (extended) | 2

<a id="ConfFile" name="ConfFile"></a>
## 4 Configuration file

The DRBDmon configuration file is stored in the user’s home directory.

Linux:
`${HOME}/.drbdmon.ces`

Windows:
`%APPDATA%/DRBDmon/drbdmon.ces`

The configuration file is in configuration entry store (CES) format. The CES format is a binary format that is not designed to be modified manually. Use the DRBDmon CLI commands to create or modify DRBDmon configuration files. Refer to developer documentation for the DRBDmon CES format if you need to develop software that creates or modifies DRBDmon configuration files.

Configuration files can be copied/distributed to other user profiles and other nodes.

DRBDmon can work with configuration files of older or newer versions of DRBDmon, as long as the same major version of configuration file format is used. However, if an older version of DRBDmon is used to modify a configuration file from a newer version of DRBDmon, any configuration entries known to the newer version of DRBDmon, but not known to the older version of DRBDmon, will be discarded.
