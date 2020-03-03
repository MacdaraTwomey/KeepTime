
#define ICON_SIZE 32

// Some icons glitch out on some sizes, maybe getting the icon sizes up/down for icons without that specific size. But good ones like firefox are built in with lots of sizes. 
// NOTE: Can test this by looking at the colour of monitor.exe icon

// dark Blue   256 (used for large and extra large icon in explorer in my Win10)
// light blue  128
// pink        96
// orange      64
// Red         48 (used for medium icon in explorer in my Win10)
// Dark green  40
// light Green 32 (used for 'content' icon in explorer, alt-tab, and taskbar in my Win10)
// Yellow      24
// Brown       22
// Purple      16 (used for small icon in explorer, window top left icon in my Win10)
// medium grey 14
// White cloudy/grey blue 10
// Zebra       8

// Icon sizes: from chromium
// 8,    Recommended by the MSDN as a nice to have icon size.
// 10,   Used by the Shell (e.g. for shortcuts).
// 14,   Recommended by the MSDN as a nice to have icon size.
// 16,   Toolbar, Application and Shell icon sizes.
// 22,   Recommended by the MSDN as a nice to have icon size.
// 24,   Used by the Shell (e.g. for shortcuts).
// 32,   Toolbar, Dialog and Wizard icon size.
// 40,   Quick Launch.
// 48,   Alt+Tab icon size.
// 64,   Recommended by the MSDN as a nice to have icon size.
// 96,   Recommended by the MSDN as a nice to have icon size.
// 128   Used by the Shell (e.g. for shortcuts).



Simple_Bitmap
win32_bitmap_to_simple_bitmap(BITMAP *win32_bitmap)
{
    rvl_assert(win32_bitmap->bmType == 0);
    rvl_assert(win32_bitmap->bmHeight > 0);
    rvl_assert(win32_bitmap->bmHeight > 0);
    rvl_assert(win32_bitmap->bmBitsPixel == 32);
    rvl_assert(win32_bitmap->bmBits);
    
    Simple_Bitmap simple_bitmap = {};
    simple_bitmap.width = win32_bitmap->bmWidth;
    simple_bitmap.height = win32_bitmap->bmHeight;
    simple_bitmap.pixels = (u32 *)xalloc(simple_bitmap.width * simple_bitmap.height * simple_bitmap.BYTES_PER_PIXEL);
    
    // BITMAP is bottom up with a positive height.
    u32 *dest = (u32 *)simple_bitmap.pixels;
    u8 *src_row = (u8 *)win32_bitmap->bmBits + (win32_bitmap->bmWidthBytes * (win32_bitmap->bmHeight-1));
    for (int y = 0; y < win32_bitmap->bmHeight; ++y)
    {
        u32 *src = (u32 *)src_row;
        for (int x = 0; x < win32_bitmap->bmWidth; ++x)
        {
            *dest++ = *src++;
        }
        
        src_row -= win32_bitmap->bmWidthBytes;
    }
    
    return simple_bitmap;
}


bool
win32_bitmap_has_alpha_component(BITMAP *bitmap)
{
    u8 *row = (u8 *)bitmap->bmBits;
    for (int y = 0; y < bitmap->bmHeight; ++y)
    {
        u32 *pixels = (u32 *)row;
        for (int x = 0; x < bitmap->bmWidth; ++x)
        {
            u32 pixel = *pixels++;
            if (pixel & 0xFF000000) return true;
        }
        
        row += bitmap->bmWidthBytes;
    }
    
    return false;
}



Simple_Bitmap
get_icon_bitmap(HICON icon)
{
    // TODO: Get icon from UWP:
    // Process is wrapped in a parent process, and can't extract icons from the child, not sure about parent
    // SO might need to use SHLoadIndirectString, GetPackageInfo could be helpful.
    
    
    // Info from Chromium source: ui/gfx/icon_itil.cc
    // https://chromium.googlesource.com/chromium/chromium/+/master/ui/gfx/icon_util.cc
    
    // THe AND mask of the icon specifies the transparency of each pixel, where one
    // each bit represents one pixel. The XOR mask contains the images pixels,
    // and possibly it's own alpha channel, meaning the AND mask isn't needed.
    
    // Icon sizes
    // 8,    Recommended by the MSDN as a nice to have icon size.
    // 10,   Used by the Shell (e.g. for shortcuts).
    // 14,   Recommended by the MSDN as a nice to have icon size.
    // 16,   Toolbar, Application and Shell icon sizes.
    // 22,   Recommended by the MSDN as a nice to have icon size.
    // 24,   Used by the Shell (e.g. for shortcuts).
    // 32,   Toolbar, Dialog and Wizard icon size.
    // 40,   Quick Launch.
    // 48,   Alt+Tab icon size.
    // 64,   Recommended by the MSDN as a nice to have icon size.
    // 96,   Recommended by the MSDN as a nice to have icon size.
    // 128   Used by the Shell (e.g. for shortcuts).
    
    // Mostly copied from raymond chen article
    // https://devblogs.microsoft.com/oldnewthing/20101020-00/?p=12493
    
    //GetIconInfoEx creates bitmaps for the hbmMask and hbmCol or members of ICONINFOEX. The calling application must manage these bitmaps and delete them when they are no longer necessary.
    
    Simple_Bitmap simple_bitmap = {};
    
    // If this structure defines a color icon, this mask only defines the AND bitmask of the icon.
    ICONINFO ii;
    
    // These bitmaps are stored as bottom up bitmaps, so the bottom row of image is stored in the first 'row' in memory.
    BITMAP bitmap;
    BITMAP mask;
    HBITMAP mask_handle = 0;
    HBITMAP colours_handle = 0;
    
    if (GetIconInfo(icon, &ii))
    {
        // It is necessary to call DestroyObject for icons and cursors created with CopyImage
        colours_handle = (HBITMAP)CopyImage(ii.hbmColor, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        mask_handle = (HBITMAP)CopyImage(ii.hbmMask, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION);
        
        // When this is done without CopyImage the bitmaps don't have any .bits memory allocated
        //int result_1 = GetObject(ii.hbmColor, sizeof(BITMAP), &bitmap);
        //int result_2 = GetObject(ii.hbmMask, sizeof(BITMAP), &mask);
        if (colours_handle && mask_handle)
        {
            int result_1 = GetObject(colours_handle, sizeof(BITMAP), &bitmap) == sizeof(BITMAP);
            int result_2 = GetObject(mask_handle, sizeof(BITMAP), &mask) == sizeof(BITMAP);
            if (result_1 && result_2)
            {
                // Only apply AND mask if the icon image didn't have an alpha channel
                if (!win32_bitmap_has_alpha_component(&bitmap))
                {
                    u8 *row = (u8 *)bitmap.bmBits;
                    u8 *bits = (u8 *)mask.bmBits;
                    int i = 0;
                    for (int y = 0; y < bitmap.bmHeight; ++y)
                    {
                        u32 *dest = (u32 *)row;
                        for (int x = 0; x < bitmap.bmWidth; ++x)
                        {
                            if(*bits & (1 << 7-i))
                            {
                                // Pixel should be transparent
                                u32 a = 0x00FFFFFF;
                                *dest &= a;
                            }
                            else
                            {
                                // Pixel should be visible
                                u32 a = 0xFF000000;
                                *dest |= a;
                            }
                            
                            ++i;
                            if (i == 8)
                            {
                                ++bits;
                                i = 0;
                            }
                            
                            
                            dest++;
                        }
                        
                        row += bitmap.bmWidthBytes;
                    }
                }
                
                simple_bitmap = win32_bitmap_to_simple_bitmap(&bitmap);
            }
        }
        
    }
    
    // Destroy HBITMAPs
    if (colours_handle)  DeleteObject(colours_handle);
    if (mask_handle) DeleteObject(mask_handle);
    
    if (ii.hbmMask)  DeleteObject(ii.hbmMask);
    if (ii.hbmColor) DeleteObject(ii.hbmColor);
    
    return simple_bitmap;
};



bool
get_icon_from_executable(char *path, u32 size, Simple_Bitmap *icon_bitmap, bool load_default_on_failure = true)
{
    rvl_assert(path);
    rvl_assert(icon_bitmap);
    rvl_assert(size > 0);
    
    HICON small_icon = 0;
    HICON icon = 0;
    
    //icon = ExtractIconA(instance, path, 0);
    //if (icon == NULL || icon == (HICON)1) printf("ERROR");
    
    if(SHDefExtractIconA(path, 0, 0, &icon, &small_icon, size) != S_OK)
    {
        // NOTE: Show me that path was actually wrong and it wasn't just failed icon extraction.
        // If we can load the executable, it means we probably should be able to get the icon
        //rvl_assert(!LoadLibraryA(path)); // TODO: Enable this when we support UWP icons
        
        if (load_default_on_failure)
        {
            icon = LoadIconA(NULL, IDI_APPLICATION);
            if (!icon) 
            {
                return false;
            }
        }
    }
    
    Simple_Bitmap bitmap;
    if (icon)
    {
        bitmap = get_icon_bitmap(icon);
    }
    else if (small_icon && !icon)
    {
        bitmap = get_icon_bitmap(small_icon);
    }
    
    if (icon) DestroyIcon(icon);
    if (small_icon) DestroyIcon(small_icon);
    
    
    if (bitmap.pixels && bitmap.width > 0 && bitmap.height > 0)
    {
        *icon_bitmap = bitmap;
        return true;
    }
    else
    {
        if (bitmap.pixels) free(bitmap.pixels);
        return false;
    }
}



// Thos stuff cam get low quality versions of the same icons that extracticonA can get
#if 0
struct Ico_Res
{
    bool set;
    char *str;
    DWORD id;
    DWORD type;
};

//Ico_Res resources[1000] = {};

BOOL CALLBACK MyEnumProcedure( HANDLE  hModule, LPCTSTR  lpszType, LPTSTR  lpszName, LONG  lParam )
{
    Ico_Res *ico_res;
    
    // Type 0x0e means 14 which might be     RT_GROUP_ICON = (RT_ICON) + 11) = 3 + 11 = 14
    for (int i = 0; i < 1000; ++i)
    {
        if (!resources[i].set)
        {
            ico_res = resources + i;
            ico_res->set = true;
            break;
        }
    }
    
    ico_res->type = (DWORD)lpszType;
    
    // Name is from MAKEINTRESOURCE()
    if( HIWORD(lpszName) == 0 )
    {
        ico_res->id = (DWORD)lpszName;
    }
    else
    {
        size_t len = strlen(lpszName);
        ico_res->str = (char *)malloc(len + 1);
        strcpy(ico_res->str, lpszName);
    }
    
    return TRUE;
}

HICON
GetIconFromInstance( HINSTANCE hInstance, LPTSTR nIndex )
{
    HICON	hIcon = NULL;
    HRSRC	hRsrc = NULL;
    HGLOBAL	hGlobal = NULL;
    LPVOID	lpRes = NULL;
    int    	nID;
    
    // Find the group icon
    if( (hRsrc = FindResource( hInstance, nIndex, RT_GROUP_ICON )) == NULL )
        return NULL;
    if( (hGlobal = LoadResource( hInstance, hRsrc )) == NULL )
        return NULL;
    if( (lpRes = LockResource(hGlobal)) == NULL )
        return NULL;
    
    // Find this particular image
    nID = LookupIconIdFromDirectory( (PBYTE)lpRes, TRUE );
    if( (hRsrc = FindResource( hInstance, MAKEINTRESOURCE(nID), RT_ICON )) == NULL )
        return NULL;
    if( (hGlobal = LoadResource( hInstance, hRsrc )) == NULL )
        return NULL;
    if( (lpRes = LockResource(hGlobal)) == NULL )
        return NULL;
    // Let the OS make us an icon
    hIcon = CreateIconFromResource( (PBYTE)lpRes, SizeofResource(hInstance,hRsrc), TRUE, 0x00030000 );
    return hIcon;
}

/*
  Icon images are stored in ICO files and as resources in EXEs and DLLs in near
DIB format - a BITMAPINFO followed by XOR bits followed by AND bits. A block
of memory like this can be passed to CreateIconFromResourceEx() to create a
HICON from the resource. This API expects the bmiHeader.biHeight member of the
BITMAPINFO to be the sum of the heights of the XOR and AND masks. Further,
this API is not implemented on NT at this time, so CreateIconFromResource()
must be used instead
*/

HMODULE module = LoadLibraryA("firefox....");

HINSTANCE inst = (HINSTANCE)module;


EnumResourceNames(inst, RT_GROUP_ICON, (ENUMRESNAMEPROC)MyEnumProcedure, (LPARAM)0);

ICONINFO    IconInfo;
BITMAP    	bm;
HICON ic;

for (int i = 0; i < 10000 && resources[i].set; ++i)
{
    if (resources[i].str)
    {
        ic = GetIconFromInstance(inst, resources[i].str);
    }
    else
    {
        ic = GetIconFromInstance(inst, (LPTSTR)resources[i].id);
    }
    
    if (ic) {
        tprint("%", i);
    }
}


GetIconInfo( ic, &IconInfo );
GetObject( IconInfo.hbmColor, sizeof(BITMAP), &bm );

Simple_Bitmap bitmap = get_icon_bitmap(ic);
draw_simple_bitmap(&global_screen_buffer, &bitmap, 0, 0);

#endif







