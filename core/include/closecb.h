#ifndef __CLOSECB__
#define __CLOSECB__ 1

#include "metaldio.h"

#pragma pack(1)
struct closecb {
  int last_entry:1;
  int opts:7;       /* CLOSE TYPE: 0x00=TYPE=T (keep DD), 0x40=TYPE=I (free DD) */
  int reserved:24;
  void* PTR32 dcb24;
};
#pragma pack(pop)

/* TYPE=I atomically closes the DCB and frees the DD inside SVC 20, removing
 * the TIOT entry before returning.  Required for BPAM OUTPUT on a plain PDS
 * to avoid DYNFREE ERCO=0x0b (DEB still registered after TYPE=T close).     */
#define CLOSE_TYPE_T  0x00
#define CLOSE_TYPE_I  0x40

#endif
