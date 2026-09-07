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

/* s99eopts all-zero: s99ermsg=1 demands a buffer via s99emsgp; NULL there makes IEFDB476 reject the RBX with rc=0x0C, hiding every real SVC99 error. */
static const struct s99_rbx s99rbxtemplate = {"S99RBX",S99RBXVR,{0,0,0,0,0,0,0},0,0,0};

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

  /* DIAG: dump the text unit we are about to free so we can verify key/length/value before SVC99. */
  debug(opts, "pre-ddfree: dd.__verb(s99tukey)=0x%04x (expect DUNDDNAM=0x0001) S99VRBUN=0x%02x dd.s99tulng=%d dd.s99tupar='%.*s'\n",
        dd->s99tukey, (unsigned int)S99VRBUN, (int)dd->s99tulng, (int)dd->s99tulng, dd->s99tupar);

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dd );
  if (!parms) {
    errmsg(opts, "Unable to initialize SVC99 (DYNFREE) control blocks\n");
    return 16;
  }

  /* DIAG: confirm the live RBX EOPTS and EMSGP fields after s99_init (stale heap would show EOPTS!=0x00 or bad EMSGP here). */
  {
    struct s99_rbx *rbx = parms->s99s99x;
    unsigned char eopts = rbx ? *((unsigned char*)&rbx->s99eopts) : 0xFF;
    void *emsgp         = rbx ? rbx->s99emsgp : (void*)0xDEAD;
    debug(opts, "ddfree: key=0x%04x num=%d lng=%d par='%.*s'\n",
          dd->s99tukey, (int)dd->s99tunum, (int)dd->s99tulng,
          (int)dd->s99tulng, dd->s99tupar);
    debug(opts, "ddfree: txtpp[0] key=0x%04x num=%d lng=%d par='%.*s'\n",
          ((struct s99_common_text_unit*)parms->s99txtpp[0])->s99tukey,
          (int)((struct s99_common_text_unit*)parms->s99txtpp[0])->s99tunum,
          (int)((struct s99_common_text_unit*)parms->s99txtpp[0])->s99tulng,
          (int)((struct s99_common_text_unit*)parms->s99txtpp[0])->s99tulng,
          ((struct s99_common_text_unit*)parms->s99txtpp[0])->s99tupar);
    debug(opts, "ddfree: SVC99 RB verb=%d rbln=%d txtpp=%p\n",
          (int)parms->s99verb, (int)parms->s99rbln, (void*)parms->s99txtpp);
    /* CRITICAL CHECK: EOPTS=0x40 with EMSGP=NULL is the exact condition that causes IEFDB476 rc=0x0C / error=0x03A8. */
    debug(opts, "ddfree: live RBX EOPTS=0x%02x EMSGP=%p (EOPTS&0x40 with NULL EMSGP → 0x03A8)\n",
          (unsigned int)eopts, emsgp);
  }

  rc = S99(parms);
  if (rc) {
    /* Always dump on failure (not just DEBUG) so the bad RBX fields are visible in production logs. */
    s99_fmt_dmp(opts, parms);
    debug(opts, "ddfree: S99 rc=%d error=0x%04x info=0x%04x\n",
          rc, (unsigned short)parms->s99error, (unsigned short)parms->s99info);
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
