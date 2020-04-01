#ifndef ICON_H
#define ICON_H


#define ICON_SIZE 32

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
};


#endif //ICON_H
