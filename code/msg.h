#pragma once

static constexpr u32 MessageGUID = 0xab8d62f2;

enum Message_Type
{
    MSG_WRITE_TO_FILE = 0,
};

struct Message
{
    u32 GUID;
    Message_Type type;
    union {
        char data[16];
    };
};
