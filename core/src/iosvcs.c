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

  /* Snapshot error/info NOW before s99_prt_msg (or anything else) can
   * overwrite the request block fields.                                 */
  unsigned short snap_error = parms->s99error;
  unsigned short snap_info  = parms->s99info;

  if (rc) {
    /* s99_prt_msg emits verb/rc/error/info then tries IEFDB476.
     * s99_fmt_dmp (full RB hex dump) fires when debug is on.           */
    s99_prt_msg(opts, parms, rc);
    s99_free(parms);
    return IOSVC_ERR_SVC99_ALLOC_FAILURE;
  }

  debug(opts, "dsdd_alloc: S99 rc=%d error=0x%04x info=0x%04x\n",
        rc, (unsigned int)snap_error, (unsigned int)snap_info);

  if (snap_error == 0x1708) {
    /* SVC99 reused a pre-existing DD; we don't own it, so skip the free. */
    debug(opts, "dsdd_alloc: error=0x1708 - borrowed DD, returning IOSVC_ERR_SVC99_BORROWED_DD\n");
    s99_free(parms);
    return IOSVC_ERR_SVC99_BORROWED_DD;
  }

  /* Copy the DD name back to the caller's text unit ONLY when DALRTDDN
   * (key 0x55) was used — that is when SVC99 writes the assigned system
   * DD name into the text unit.  For DALDDNAM (key 0x01) the caller
   * already knows the name and the text unit is not modified by SVC99.    */
  if (dd->s99tukey == DALRTDDN) {
    struct s99_common_text_unit* ddout = (struct s99_common_text_unit*) parms->s99txtpp[1];
    dd->s99tulng = ddout->s99tulng;
    memcpy(dd->s99tupar, ddout->s99tupar, dd->s99tulng);
  }

  s99_free(parms);
  return IOSVC_ERR_NOERROR;
}

int ddfree(struct s99_common_text_unit* dd, const DBG_Opts* opts)
{
  struct s99rb* PTR32 parms;
  enum s99_verb verb = S99VRBUN;
  struct s99_flag1 s99flag1 = {0};
  struct s99_flag2 s99flag2 = {0};
  /* s99tioex MUST remain 0 for BPAM DD unallocation.
   *
   * When s99tioex=1, DYNFREE enables extended in-use validation: it checks
   * both the TIOT and XTIOT and also inspects the DEB chain to confirm the
   * DCB is fully decoupled from the DD.  On a PDS (non-PDSE), the BPAM
   * CLOSE macro returns rc=0 but the DEB may still be chained to the TIOT
   * entry for a brief window while the access method finishes internal
   * cleanup.  DYNFREE with s99tioex=1 sees the DEB and returns
   * rc=12 / error=0x03a8 / ERCO=0x0b ("DD in use by active DCB"), leaving
   * the dataset allocated and blocking any subsequent fopen() from dmod/dsed.
   *
   * With s99tioex=0, DYNFREE uses the classic TIOT-only path (DUNUNALC
   * semantics) which does not check the DEB in-use state for non-SMS
   * datasets.  This is the correct and standard behaviour immediately after
   * BPAM CLOSE on both PDS and PDSE.                                       */
  size_t num_text_units = 1;
  int rc;
  struct s99_rbx s99rbx = s99rbxtemplate;

  debug(opts, "ddfree: DD='%.*s' len=%d s99tioex=%d\n",
        (int)dd->s99tulng, dd->s99tupar, (int)dd->s99tulng,
        (int)s99flag2.s99tioex);

  parms = s99_init(verb, s99flag1, s99flag2, &s99rbx, num_text_units, dd );
  if (!parms) {
    errmsg(opts, "Unable to initialize SVC99 (DYNFREE) control blocks\n");
    return 16;
  }
  rc = S99(parms);
  if (rc) {
    /* Snapshot ERCO from the RBX before s99_prt_msg can touch the block.
     * ERCO is the extended reason code written by DYNFREE itself:
     *   0x0b = DD in use by another task / existing allocation in this AS
     *   0x08 = DDname not found in TIOT or XTIOT
     *   0x04 = DD allocated to a different step
     * This is the definitive "why" when error=0x03a8 (IEFDB476 unavailable). */
    unsigned char snap_erco = (parms->s99s99x) ? parms->s99s99x->s99erco : 0xFF;
    debug(opts, "ddfree: S99 DYNFREE failed rc=0x%x error=0x%04x info=0x%04x ERCO=0x%02x\n",
          (unsigned int)rc,
          (unsigned int)parms->s99error,
          (unsigned int)parms->s99info,
          (unsigned int)snap_erco);
    /* s99_prt_msg emits verb/rc/error/info then tries IEFDB476.
     * s99_fmt_dmp (full RB hex dump) fires when debug is on.           */
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
