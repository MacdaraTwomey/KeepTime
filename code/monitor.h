#pragma once

#include "base.h"
#include "monitor_string.h"
#include "date.h"
#include "helper.h"
#include "apps.h"
#include "gui.h"

#define PROGRAM_NAME "KeepTime"
#define VERSION_STRING "0.0.1"
#define NAME_STRING "Mac"
#define LICENSE_STRING "MIT Licence"


struct monitor_state
{
    bool IsInitialised;
    
    arena PermanentArena;
    arena TemporaryArena;
    
    string_map AppNameMap;
    u32 CurrentID;
    
    // TODO:
    // Will have holes in array when an website ID takes up a slot
    // Doesn't check acount allocation capacity
    c_string *ProgramPaths; 
    u32 ProgramCount;
    
    record *Records;
    u64 RecordCount;
    
    settings Settings;
    
    gui GUI;
};
