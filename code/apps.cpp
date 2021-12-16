
#include "date.h"
#include "base.h"
#include "platform.h"
#include "monitor_string.h"
#include "helper.h"
#include "monitor.h"
#include "apps.h"

date::sys_days GetLocalTime()
{
    time_t rawtime;
    time( &rawtime );
    
    // Looks in TZ database, not threadsafe
    struct tm *info;
    info = localtime( &rawtime );
    auto now = date::sys_days{
        date::month{(unsigned)(info->tm_mon + 1)}/date::day{(unsigned)info->tm_mday}/date::year{info->tm_year + 1900}};
    
    return now;
}

void SetBit(u32 *A, u32 BitIndex)
{
    Assert(BitIndex < 32);
    *A |= (1 << BitIndex);
}

app_id GenerateAppID(u32 *CurrentID, app_type AppType)
{
    Assert(*CurrentID != 0);
    Assert(*CurrentID < APP_TYPE_BIT_MASK);
    
    app_id ID = *CurrentID++;
    if (AppType == app_type::WEBSITE) SetBit(&ID, APP_TYPE_BIT_INDEX); 
    
    return ID;
}

u32 IndexFromAppID(app_id ID)
{
    Assert(ID != 0);
    
    // Get bottom 31 bits of id
    u32 Index = ID & (~APP_TYPE_BIT_MASK);
    
    return Index;
}

bool IsWebsite(app_id ID)
{
    return ID & (1 << APP_TYPE_BIT_INDEX);
}

bool IsProgram(app_id ID)
{
    return !IsWebsite(ID);
}


#define GLCheck(x) GLClearError(); x; GLLogCall(#x, __FILE__, __LINE__)

void
GLClearError()
{
    while (glGetError() != GL_NO_ERROR);
}

bool
GLLogCall(const char *function, const char *file, int line)
{
    while (GLenum error = glGetError())
    {
        printf("[OpenGL Error] (%u) %s %s: Line %i\n", error, function, file, line);
        return false;
    }
    
    return true;
}

u32 OpenGLCreateTexture(bitmap Bitmap)
{
#define GL_BGRA                           0x80E1
    
    GLuint ImageTexture;
    GLCheck(glGenTextures(1, &ImageTexture)); // I think this can fail if out of texture mem
    
    GLCheck(glBindTexture(GL_TEXTURE_2D, ImageTexture));
    
    GLCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    GLCheck(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    
    // Can be PACK for glReadPixels or UNPCK GL_UNPACK_ROW_LENGTH
    
    // This sets the number of pixels in the row for the glTexImage2D to expect, good 
    //glPixelStorei(GL_UNPACK_ROW_LENGTH, 24);
    
    // Alignment of the first pixel in each row
    GLCheck(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    
    // You create storage for a Texture and upload pixels to it with glTexImage2D (or similar functions, as appropriate to the type of texture). If your program crashes during the upload, or diagonal lines appear in the resulting image, this is because the alignment of each horizontal line of your pixel array is not multiple of 4. This typically happens to users loading an image that is of the RGB or BGR format (for example, 24 BPP images), depending on the source of your image data. 
    
    GLCheck(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Bitmap.Width, Bitmap.Height, 0, GL_BGRA, GL_UNSIGNED_BYTE, Bitmap.Pixels));
    
    return ImageTexture;
}

icon_asset *GetAppIconAsset(monitor_state *State, gui *GUI, app_id ID)
{
    if (IsWebsite(ID))
    {
        //get_favicon_from_website(url);
        
        icon_asset *DefaultIcon = &GUI->Icons[DEFAULT_WEBSITE_ICON_INDEX];
        Assert(DefaultIcon->TextureHandle != 0);
        return DefaultIcon;
    }
    
    if (IsProgram(ID))
    {
        u32 Index = IndexFromAppID(ID);
        
        bool IconNotLoaded = Index >= GUI->IconCount;
        if (IconNotLoaded)
        {
            string Path = State->ProgramPaths[Index];
            Assert(Path.Length > 0);
            
            bitmap Bitmap = {};
            bool Success = PlatformGetIconFromExecutable(&GUI->Arena, Path.Str, ICON_SIZE, &Bitmap);
            if (Success)
            {
                // TODO: Maybe mark old paths that couldn't get correct icon for deletion or to stop trying to get icon, or assign a default invisible icon
                icon_asset *Icon = &GUI->Icons[Index];
                
                Icon->TextureHandle = OpenGLCreateTexture(Bitmap);
                Icon->Width = Bitmap.Width; // unnecessary
                Icon->Height = Bitmap.Height; // unnecessary
                Icon->Loaded = true;
                
                GUI->IconCount += 1;
                
                return Icon;
            }
        }
        else
        {
            // Icon already loaded
            icon_asset *Icon = &GUI->Icons[Index];
            Assert(Icon->TextureHandle != 0);
            return Icon;
        }
    }
    
    icon_asset *DefaultIcon = &GUI->Icons[DEFAULT_PROGRAM_ICON_INDEX];
    Assert(DefaultIcon->TextureHandle != 0);
    return DefaultIcon;
}


app_id GetAppID(arena *Arena, 
                string_map *AppNameMap, 
                c_string *ProgramPaths, u32 *ProgramCount, 
                u32 *CurrentID, app_info Info)
{
    app_id ID = 0;
    
    // Share hash table
    string_map_entry *Entry = StringMapGet(AppNameMap, Info.Name);
    if (!Entry)
    {
        // Error no space in hash table (should never realistically happen)
        // Return ID of NULL App?
        Assert(0);
    }
    else if (StringMapEntryExists(Entry))
    {
        ID = Entry->Value;
    }
    else 
    {
        ID = GenerateAppID(CurrentID, Info.Type);
        
        // Copy string
        c_string AppName = PushStringNullTerminated(Arena, Info.Name);
        
        StringMapAdd(AppNameMap, Entry, AppName, ID);
        
        if (Info.Type == app_type::PROGRAM)
        {
            ProgramPaths[IndexFromAppID(ID)] = AppName;
            *ProgramCount += 1;
        }
    }
    
    Assert((Info.Type == app_type::PROGRAM && IsProgram(ID)) ||
           (Info.Type == app_type::WEBSITE && IsWebsite(ID)));
    
    return ID;
}


void UpdateRecords(record *Records, u64 *RecordCount, app_id ID, date::sys_days Date, s64 DurationMicroseconds)
{
    // Loop backwards searching through Records with passed Date for record marching our ID
    bool RecordFound = false;
    
    for (u64 RecordIndex = *RecordCount - 1; 
         RecordIndex != static_cast<u64>(-1); 
         --RecordIndex)
    {
        record *ExistingRecord = Records + RecordIndex;
        if (ExistingRecord->Date != Date)
        {
            break;
        }
        
        if (ExistingRecord->ID == ID)
        {
            ExistingRecord->DurationMicroseconds += DurationMicroseconds;
            RecordFound = true;
            break;
        }
    }
    
    if (!RecordFound)
    {
        record *NewRecord = Records + *RecordCount;
        NewRecord->ID = ID;
        NewRecord->Date = Date;
        NewRecord->DurationMicroseconds = DurationMicroseconds;
        
        *RecordCount += 1;
    }
}
