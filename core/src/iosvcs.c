#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metaldio.h"
#include "dio.h"
#include "mem.h"
#include "ihadcb.h"
#include "iosvcs.h"
#include "s99.h"
#include "msg.h"

#define DD_SYSTEM "????????"
#define ERRNO_NONEXISTANT_FILE (67)
#define DIO_MSG_BUFF_LEN (4095)

static const struct s99_rbx s99rbxtemplate = {"S99RBX",S99RBXVR,{0,1,0,0,0,0,0},0,0,0};

int dsdd_alloc(struct s99_common_text_unit* dsn, struct s99_common_text_unit* dd, struct s99_common_text_unit* disp, const DBG_Opts* opts)
{
  struct s99rb* PTR32 parms;
  enum s99_verb verb = S99VRBAL;
  struct s99_flag1 s99flag1 = {0};
  struct s99_flag2 s99flag2 = {0};
  size_t num_text_units = 3;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dsn, dd, disp );
  if (!parms) {
    return IOSVC_ERR_SVC99INIT_ALLOC_FAILURE;
  }
  rc = S99(parms);
  if (rc) {
    if (opts && opts->debug) {
      s99_fmt_dmp(opts, parms);
    }
    s99_prt_msg(opts, parms, rc);
    return IOSVC_ERR_SVC99_ALLOC_FAILURE;
  }

  struct s99_common_text_unit* ddout = (struct s99_common_text_unit*) parms->s99txtpp[1];
  dd->s99tulng = ddout->s99tulng;
  memcpy(dd->s99tupar, ddout->s99tupar, dd->s99tulng);

  s99_free(parms);
  return IOSVC_ERR_NOERROR;
}

int ddfree(struct s99_common_text_unit* dd, const DBG_Opts* opts)
{
  struct s99rb* PTR32 parms;
  enum s99_verb verb = S99VRBUN;
  struct s99_flag1 s99flag1 = {0};
  struct s99_flag2 s99flag2 = {0};
  s99flag2.s99tioex = 1; /* search XTIOT as well as classic TIOT */
  size_t num_text_units = 1;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  /* DBG: print the DD name and key being passed to SVC 99 UNFREE so we
   * can compare with what alloc_pds() stored in the handle at open time. */
  errmsg(opts, "IOSVCS ddfree: entry DUNDDNAM key:0x%04X len:%d DD:[%.*s]\n",
         (unsigned)dd->s99tukey, (int)dd->s99tulng,
         (int)dd->s99tulng, dd->s99tupar);

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dd );
  if (!parms) {
    errmsg(opts, "Unable to initialize SVC99 (DYNFREE) control blocks\n");
    return 16;
  }
  rc = S99(parms);
  if (rc) {
    /* DBG: unconditionally emit S99ERROR and S99INFO so the root cause
     * is visible even without debug mode.  S99ERROR 0x0230 = "DD not in
     * TIOT/XTIOT" (S99ECSVU), which would mean the DD was already freed
     * or was never successfully allocated.                              */
    errmsg(opts, "IOSVCS ddfree: SVC99 UNFREE failed rc:%d S99ERROR:0x%04X S99INFO:0x%04X\n",
           rc, (unsigned)parms->s99error, (unsigned)parms->s99info);
    if (opts && opts->debug) {
      s99_fmt_dmp(opts, parms);
    }
    s99_prt_msg(opts, parms, rc);
    s99_free(parms);
    return rc;
  }

  errmsg(opts, "IOSVCS ddfree: SVC99 UNFREE OK for DD:[%.*s]\n",
         (int)dd->s99tulng, dd->s99tupar);

  s99_free(parms);
  return 0;
}

int init_dsnam_text_unit(const char* dsname, struct s99_common_text_unit* dsn, const DBG_Opts* opts)
{
  size_t dsname_len = (dsname == NULL) ? 0 : strlen(dsname);
  if (dsname == NULL || dsname_len == 0 || dsname_len > DS_MAX) {
    errmsg(opts, "Dataset Name <%.*s> is invalid\n", dsname_len, dsname);
    return 8;
  }

  dsn->s99tulng = dsname_len;
  memcpy(dsn->s99tupar, dsname, dsname_len);
  return 0;
}
