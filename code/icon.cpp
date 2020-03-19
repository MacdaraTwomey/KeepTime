
#define ICON_SIZE 32

// Some icons glitch out on some sizes, maybe getting the icon sizes up/down for icons without that specific size. // But maybe good ones like firefox are built in with lots of sizes.
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


// BITMAPS for icons
#if 0
struct ICON_IMAGE
{
    // only biSize, biWidth, biHeight, biPlanes, biBitCount, biSizeImage are used others must be 0.
    // The biHeight member specifies the combined height of the XOR and AND masks.
    BITMAPINFOHEADER header;
    
    // biHeight is width*2
    
    // If biCompression equals BI_RGB and the bitmap uses 8 bpp or less, the bitmap has a color table immediately following the BITMAPINFOHEADER structure.
    // The array contains the maximum number of colors for the given bitdepth; which is, 2^biBitCount colors, as biClrUsed == 0, see notes.
    RGB_QUAD colours[2^bitcount]; // (optional)
    // u32 == RGB_QUAD
    
    // These have the same dimensions
    // XOR is first in memory, then and (PC Mag 26 Jan 1993)
    //https://books.google.com.au/books?id=LpkFEO2FG8sC&pg=PA320&lpg=PA320&dq=is+and+or+xor+mask+first+in+icon&source=bl&ots=2eFRPsG5No&sig=ACfU3U17mCL0Jt3INruonl4_MevZEPZiQA&hl=en&sa=X&ved=2ahUKEwi2rNSx6JfoAhVawTgGHT6wA8kQ6AEwDXoECAoQAQ#v=onepage&q=is%20and%20or%20xor%20mask%20first%20in%20icon&f=false
    u8 XOR[w * h/2 * bitcount/8]; // number of bytes in XOR mask, depends bpp in header
    u8 AND[w * h/2 * 1/8];        // 1bpp, number of bytes depends on member in header
};

// For BITMAPINFOHEADER biCompression
typedef  enum
{
    BI_RGB = 0x0000, // uncompressed
    BI_RLE8 = 0x0001,
    BI_RLE4 = 0x0002,
    BI_BITFIELDS = 0x0003,
    BI_JPEG = 0x0004,
    BI_PNG = 0x0005,
    BI_CMYK = 0x000B,
    BI_CMYKRLE8 = 0x000C,
    BI_CMYKRLE4 = 0x000D
};
#endif

// biBitcount is the number of bits per pixel
// For 16-bpp bitmaps, if biCompression equals BI_RGB, the format is always RGB 555. If biCompression equals BI_BITFIELDS, the format is either RGB 555 or RGB 565.
// If biCompression equals BI_BITFIELDS, the bitmap uses three DWORD color masks (red, green, and blue, respectively), which specify the byte layout of the pixels. The 1 bits in each mask indicate the bits for that color within the pixel.

// Color Tables
// NOTE:
// As biClrUsed is unsed for icons it is always zero, the array contains the maximum number of colors for the given bitdepth; that is, 2^biBitCount colors.


// Stride
// For uncompressed RGB formats
// stride = ((((biWidth * biBitCount) + 31) & ~31) >> 3)

// --------------------------------------
// Example case www.docs.windows.com icon:
// ICONDIR
// 6 ICONDIRENTRYs

// 1st entry
// width and heigh 128
// colour count 16
// bitcount 0
// planes 0
// size 10344
// offst 102

// 1st BITMAPINFOHEADER
// width 128, height 256  (128 for AND 128 for XOR)
// planes 1
// bitcount 4      (so colour count = 1 << 1 * 4 = 16)
// compression 0   (BI_RGB)
// biSizeImage  10240

// calculation probably was:
// 128*128*(1/2) w*h*(4/8)  (4bpp / 8bits per byte) - XOR mask
// + 128*128*(1/8)      (1bpp so 1/8 bytes) -  AND mask
// = 10240              (but this is != 10344 for ICONDIRENTRY.size)

// so 10344 byte size is probably composed of:
//   40 byte BITMAPINFOHEADER
//   64 byte colour table
//   10240 byte XOR and AND masks

// colour table is sizeof(RGB_QUAD) * 2^biBitcount colours = 4*16 = 64 bytes
// The table contains only 6 colours being used the rest are just 0x00000000
// black, white, and four colours of windows 'window' square

// ----------------------------------------

struct MY_RGBQUAD
{
    u8 blue;
    u8 green;
    u8 red;
    u8 reserved;
};

// Wikipedia: The bits per pixel might be set to zero, but can be inferred from the other data; specifically, if the bitmap is not PNG compressed, then the bits per pixel can be calculated based on the length of the bitmap data relative to the size of the image.

// These are structures for ICO file format, which differs slightly from format of windows icon resources
struct ICONDIRENTRY
{
    u8  width;     // Originally range was 1-255 originally but 0 is accepted to represent width of 256
    u8  height;    // Originally range was 1-255 originally but 0 is accepted to represent width of 256
    u8  colour_count; // should be equal to bColorCount = 1 << (wBitCount * wPlanes) ... but see notes
    u8  reserved;  // Must be 0
    u16 planes;    // typically not used and set to 0. PCMAG
    u16 bit_count; // typically not used and set to 0. PCMAG
    u32 size;      // in bytes of the image data
    u32 offset;    // from start of ICO file
};

struct ICONDIR
{
    u16 reserved;       // Reserved. Must always be 0.
    u16 type;           // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image.
    u16 entry_count;    // Specifies number of images in the file.
    // ICONDIRENTRY entries[count]; folled by count entries in file
};

// ICO file format:

// NOTE: colour_count should be th enumber of colours in the image (i.e)
//           bColorCount = 1 << (wBitCount * wPlanes)
// If wBitCount * wPlanes is greater than or equal to 8, then bColorCount is zero.
// But lazy people often just set it to 0, so windows autodetects this error and tries to autocorrect.

// From Raymond Chen series: The evolution of the ICO file format
// Monochrome Icons:
// A monochome icon is described by two bitmaps, called AND (or mask) and XOR (or image, or when we get to color icons, color).

// The mask (or AND) and image (or XOR) bitmaps are physically stored as one single double-height DIB. So if you actually look at the bitmap the mask is in the top half of the bitmap and the image is in the bottom.

// In terms of file format, each icon image is stored in the form of a BITMAPINFO (which itself takes the form of a BITMAPINFOHEADER followed by a color table), followed by the image pixels, followed by the mask pixels.

// The biCompression must be BI_RGB. Since this is a double-height bitmap, the biWidth is the width of the image, but the biHeight is double the image height.

// Colour Icons:
// The representation of color images in an ICO file is almost the same as the representation of monochrome images: All that changes is that the image bitmap is now in color. (The mask remains monochrome.)

// So the image format consists of a BITMAPINFOHEADER where the bmWidth is the width of the image and bmHeight is double the height of the image, followed by the bitmap color table, followed by the image pixels, followed by the mask pixels.

// Supported values for biCompression for color images are BI_RGB and (if your bitmap is 16bpp or 32bpp) BI_BITFIELDS.

// Alpha blended images:
// Windows XP introduced the ability to provide icon images which contain an 8-bit alpha channel. Up until this point, you had only a 1-bit alpha channel, represented by a mask.

// To use an alpha-blended image, just drop in a ARGB 32bpp bitmap. However, you must still provide a mask. Some people just provide a zeroed out mask, but this shouldn't be done. Because if the user calls GetIconInfo and to draw the bitmap will get an ugly result.

// PNG images:
// _NOT_ shown by BI_PNG in biCompression in BITMAPINFOHEADER, instead the PNG image just starts where the bitmap would.
// The format of a PNG-compressed image consists simply of a PNG image, starting with the PNG file signature. The image must be in 32bpp ARGB format (known to GDI+ as Pixel­Format­32bpp­ARGB). There is no BITMAP­INFO­HEADER prefix, and no monochrome mask is present.



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


bool
bitmap_has_alpha_component(u32 *pixels, int width, int height, int pitch)
{
    u8 *row = (u8 *)pixels;
    for (int y = 0; y < height; ++y)
    {
        u32 *pixels = (u32 *)row;
        for (int x = 0; x < width; ++x)
        {
            u32 pixel = *pixels++;
            if (pixel & 0xFF000000) return true;
        }
        
        row += pitch;
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
