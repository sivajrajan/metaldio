#ifndef __CLOSECB__
#define __CLOSECB__ 1

#include "metaldio.h"

#pragma pack(1)
struct closecb {
  int last_entry:1;
  int opts:7;
  int reserved:24;
  void* PTR32 dcb24;
};

#pragma pack(pop)

#define CLOSE_TYPE_T  0x00
#define CLOSE_TYPE_I  0x40

#endif
