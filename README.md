# Program Monitor

Program monitor is a simple automatic time tracking app that monitors your personal program and website usage, and displays the collected data in a summary chart. 

## Usage

To install just click the executable when you want to start tracking your program usage. 
To have the monitor run after system startup, you can place a shortcut in the windows startup folder (found by using the run command
'Win + r' then typing 'shell:startup'). 

To see your recorded data on usage, left click the Program Monitor icon in your system tray and select 'Open Barplot Window'.
From here you can select a date range to aggregate data from and display. 

In the settings menu you can choose whether you want All or None websites to be recorded, or to only record a Custom list.

![Alt text](.github/images/keeptime1.jpg?raw=true "Title")
https://raw.githubusercontent.com/{macdaratwomey}/{programmonitor}/{master}/.github/images/{program_monitor1}.{png}

## How it works



## Building

To build the program with Microsoft's Visual C++ Compiler, setup your environment by running vcvars64.bat in your visual studio installation,
then navigate to the ProgramMonitor base directory and run build.bat. 

Currently only Windows is supported.

// Need to include because of FTL
This project uses the FreeType library for font rendering
