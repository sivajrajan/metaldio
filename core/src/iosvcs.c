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
#ifdef DEBUG
    s99_fmt_dmp(opts, parms);
#endif
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
  size_t num_text_units = 1;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  /* DIAG: text unit values passed to DYNFREE — key must be DUNDDNAM=0x0001. */
  debug(opts, "ddfree: dd key=0x%04x num=%d lng=%d par='%.*s'\n",
        dd->s99tukey, (int)dd->s99tunum, (int)dd->s99tulng,
        (int)dd->s99tulng, dd->s99tupar);

  /* DIAG: template address and every RBX field in the local stack copy.
   * s99rbxtemplate is a static const — its address shows where the original lives.
   * s99rbx is the stack copy used as the source for s99_init's *rbxp = *rbxin.
   * s99ermsg=1 (eopts bit1) with emsgp=NULL is the known 0x03A8 trigger.      */
  debug(opts, "ddfree: &s99rbxtemplate=%p  &s99rbx(stack copy)=%p  sizeof(s99rbx)=%zu\n",
        (void*)&s99rbxtemplate, (void*)&s99rbx, sizeof(s99rbx));
  debug(opts, "ddfree: s99rbx eid='%c%c%c%c%c%c'(0x%02X%02X%02X%02X%02X%02X) ever=0x%02X\n",
        s99rbx.s99eid[0], s99rbx.s99eid[1], s99rbx.s99eid[2],
        s99rbx.s99eid[3], s99rbx.s99eid[4], s99rbx.s99eid[5],
        (unsigned char)s99rbx.s99eid[0], (unsigned char)s99rbx.s99eid[1],
        (unsigned char)s99rbx.s99eid[2], (unsigned char)s99rbx.s99eid[3],
        (unsigned char)s99rbx.s99eid[4], (unsigned char)s99rbx.s99eid[5],
        (unsigned char)s99rbx.s99ever);
  /* DIAG: eopts byte — s99ermsg=1 (0x40) with emsgp=NULL causes IEFDB476 rc=0x0C / error 0x03A8. */
  debug(opts, "ddfree: s99rbx eopts=0x%02X (eimsg=%d ermsg=%d elsto=%d emkey=%d emsub=%d ewtp=%d) emsgp=%p\n",
        *((unsigned char*)&s99rbx.s99eopts),
        s99rbx.s99eopts.s99eimsg, s99rbx.s99eopts.s99ermsg,
        s99rbx.s99eopts.s99elsto, s99rbx.s99eopts.s99emkey,
        s99rbx.s99eopts.s99emsub, s99rbx.s99eopts.s99ewtp,
        s99rbx.s99emsgp);

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dd );
  if (!parms) {
    errmsg(opts, "Unable to initialize SVC99 (DYNFREE) control blocks\n");
    return 16;
  }

  /* DIAG: live RBX pointer inside parms after s99_init — this is what SVC99 actually reads.
   * Compare rbxp address and eid[0] against the stack copy above to confirm the copy landed correctly. */
  {
    struct s99_rbx* PTR32 rbxp = parms->s99s99x;
    debug(opts, "ddfree: rbxp(live below-bar)=%p eid[0]=0x%02X(expect 0xE2) eopts=0x%02X emsgp=%p\n",
          (void*)rbxp,
          rbxp ? (unsigned char)rbxp->s99eid[0] : 0xFF,
          rbxp ? *((unsigned char*)&rbxp->s99eopts) : 0xFF,
          rbxp ? rbxp->s99emsgp : (void*)0xDEAD);
  }

  rc = S99(parms);
  if (rc) {
    s99_fmt_dmp(opts, parms); /* always dump on failure — not just DEBUG — so bad fields are visible */
    s99_prt_msg(opts, parms, rc);
    s99_free(parms);
    return rc;
  }

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
