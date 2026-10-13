#pragma once
#include <Windows.h>
#include <winioctl.h>

#define LUNA_DEVICE_NAME     L"\\Device\\lunadrv"
#define LUNA_SYMLINK_NAME    L"\\DosDevices\\lunadrv"
#define LUNA_USERMODE_PATH   L"\\\\.\\lunadrv"

#define LUNA_IOCTL_ATTACH  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define LUNA_IOCTL_READ    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define LUNA_IOCTL_WRITE   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define LUNA_IOCTL_DETACH  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define LUNA_IOCTL_PING    CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

#define LUNA_MAX_IO 8192

#pragma pack(push, 1)
typedef struct { unsigned int pid; } LunaAttachReq;
typedef struct { unsigned long long address; unsigned int size; } LunaIoReq;
typedef struct { unsigned int magic; } LunaPingResp;
#pragma pack(pop)

#define LUNA_PING_MAGIC 0x4C554E41u
