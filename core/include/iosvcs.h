#ifndef __IO_SERVICES__
  #define __IO_SERVICES__ 1

  #include "metaldio.h"

  #include "s99.h"
  #include "dbgopts.h"

  /*
   * Error codes from dsd_alloc
   */
   #define IOSVC_ERR_NOERROR                  0
   #define IOSVC_ERR_SVC99INIT_ALLOC_FAILURE  4
   #define IOSVC_ERR_SVC99_ALLOC_FAILURE      8

  int dsdd_alloc(struct s99_common_text_unit* dsn, struct s99_common_text_unit* dd, struct s99_common_text_unit* disp, const DBG_Opts* opts);
  /*
   * dsdd_alloc_autoclose: identical to dsdd_alloc but includes the DALCLOSE
   * text unit (key 0x58) in the SVC 99 ALLOC request.  MVS will automatically
   * unallocate the DD when the DCB opened against it is closed via the CLOSE
   * macro, eliminating the need for an explicit SVC 99 UNFREE call and
   * avoiding S99ERROR:0x03A8 on BPAM-managed DDs.
   */
  int dsdd_alloc_autoclose(struct s99_common_text_unit* dsn, struct s99_common_text_unit* dd, struct s99_common_text_unit* disp, const DBG_Opts* opts);
  int ddfree(struct s99_common_text_unit* dd, const DBG_Opts* opts);
  int init_dsnam_text_unit(const char* dsname, struct s99_common_text_unit* dsn, const DBG_Opts* opts);
#endif
