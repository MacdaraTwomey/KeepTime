# KeepTime

KeepTime is a simple automatic time tracking app that monitors your personal program and website usage, and displays the collected data in a summary chart. 

## Features

* Fine time resolution monitoring 
* Doesn't require a browser plugin
* Adjustable date range barplot, Check your program usage at different granularities
* Small memory footprint

## Usage

To install just click the executable when you want to start tracking your program usage. 
To have the program run after system startup, you can place a shortcut in the windows startup folder (found by using the run command
'Win + r' then typing 'shell:startup'). 

To see your recorded data on usage, left click the KeepTime icon in your system tray and select 'Open Barplot Window'.
From here you can select a date range to aggregate data from and display. 

![Alt text](.github/images/KeepTimeTrayIcon.JPG)


In the settings menu you can choose whether you want All or None websites to be recorded, or to only record a Custom list.

![Alt text](.github/images/KeepTimeDesktop1.jpeg)

## How it works

This program relies on the Windows accessibility API (UI Automation) to retrieve the URL from the current tab of your browser.
At the moment KeepTime only tries to get URLs from mozilla firefox.  


## Building

To build the program with Microsoft's Visual C++ Compiler, setup your environment by running vcvars64.bat in your visual studio installation,
then navigate to the KeepTime base directory and run build.bat. 

Currently only Windows is supported, though I plan to add Linux.

## Dependencies

This project uses SDL for opening a window and handling input, ImGui for rendering the UI and the FreeType library for font rendering.
These are provided as dll files in the build directory.
