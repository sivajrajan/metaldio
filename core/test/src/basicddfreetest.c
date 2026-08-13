#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "metaldio.h"
#include "mem.h"
#include "s99.h"
#include "iosvcs.h"

/*
 * basicddfreetest.c  -  Regression test for the S0C4 fix in iosvcs.c / s99.c
 *
 * Background
 * ----------
 * ddfree() (iosvcs.c) previously passed stderr (a FILE*) as the first
 * argument to s99_fmt_dmp() and s99_prt_msg(), both of which expect a
 * const DBG_Opts*.  msg.c reads opts->info_buffer from the first bytes of
 * whatever pointer is passed.  The FILE struct bytes are non-zero in that
 * position, so msg.c treated the value as a valid buffer pointer and wrote
 * into it, causing an S0C4 Protection Exception.
 *
 * Fix applied
 * -----------
 * iosvcs.c  ddfree():      s99_fmt_dmp(NULL, parms)  and
 *                           s99_prt_msg(NULL, parms, rc)
 * s99.c     s99_prt_msg(): if (opts && opts->debug) { s99_em_fmt_dmp(...) }
 *           s99_em_fmt_dmp() new helper dumps the s99_em parameter block.
 *
 * Tests
 * -----
 * Test 1 - dsdd_alloc / ddfree round-trip (success path)
 *   Allocates SYS1.MACLIB DISP=SHR and frees it.  Confirms the success
 *   path is unaffected by the change.
 *
 * Test 2 - ddfree() error path: NULL opts must not S0C4
 *   Forces SVC99 UNFREE to fail by calling ddfree() a second time on an
 *   already-freed DDname.  Before the fix this abended with S0C4.
 *   After the fix ddfree() must return non-zero and execution continues.
 *
 * Test 3 - s99_prt_msg() with opts==NULL: NULL guard must hold
 *   Calls s99_prt_msg() directly with opts==NULL and a synthetic s99rb
 *   (verb=S99VRBUN so emidnum→EMFREE, matching the ddfree() error path).
 *   Confirms that the if (opts && opts->debug) guard prevents any
 *   dereference of opts and that s99_em_fmt_dmp() is not reached.
 */

/* SYS1.MACLIB is present on every z/OS system and allocatable DISP=SHR. */
#define TEST_DSN "SYS1.MACLIB"

/* ------------------------------------------------------------------ */
static int g_passed = 0;
static int g_failed = 0;

static void record_pass(const char *name)
{
    ++g_passed;
    fprintf(stdout, "PASS: %s\n", name);
}

static void record_fail(const char *name, const char *reason)
{
    ++g_failed;
    fprintf(stderr, "FAIL: %s - %s\n", name, reason);
}

/* ------------------------------------------------------------------ */
/*
 * Test 1 - dsdd_alloc / ddfree round-trip
 *
 * Pattern taken from basicalloc.c:
 *   init_dsnam_text_unit → dsdd_alloc → capture DDname → ddfree
 * opts==NULL throughout, matching the fixed code in both iosvcs.c
 * functions.  ddfree() must return 0.
 */
static void test_alloc_free_roundtrip(void)
{
    const char *tname = "test_alloc_free_roundtrip";
    int rc;
    char ddname[8+1];

    struct s99_common_text_unit dsn   = { DALDSNAM, 1, 0, {0} };
    struct s99_common_text_unit dd    = { DALRTDDN, 1, sizeof(DD_SYSTEM)-1, DD_SYSTEM };
    struct s99_common_text_unit stats = { DALSTATS, 1, 1, {DALSTATS_SHR} };

    rc = init_dsnam_text_unit(TEST_DSN, &dsn, NULL);
    if (rc) {
        record_fail(tname, "init_dsnam_text_unit failed");
        return;
    }

    rc = dsdd_alloc(&dsn, &dd, &stats, NULL);
    if (rc) {
        record_fail(tname, "dsdd_alloc failed - SYS1.MACLIB not accessible");
        return;
    }

    memcpy(ddname, dd.s99tupar, dd.s99tulng);
    ddname[dd.s99tulng] = '\0';
    fprintf(stdout, "  [info] allocated DDname: %s -> %s\n", TEST_DSN, ddname);

    rc = ddfree(&dd);
    if (rc != 0) {
        record_fail(tname, "ddfree returned non-zero on a valid DDname");
        return;
    }

    record_pass(tname);
}

/* ------------------------------------------------------------------ */
/*
 * Test 2 - ddfree() NULL-opts error path (S0C4 regression)
 *
 * 1. Allocate a DDname.
 * 2. Free it once  (rc must be 0).
 * 3. Free it again - SVC99 UNFREE must fail (DDname gone).
 *    Before the fix: abend S0C4 inside s99_prt_msg / msg.c.
 *    After the fix:  non-zero rc returned, program survives.
 */
static void test_ddfree_null_opts_on_error(void)
{
    const char *tname = "test_ddfree_null_opts_on_error";
    int rc;

    struct s99_common_text_unit dsn   = { DALDSNAM, 1, 0, {0} };
    struct s99_common_text_unit dd    = { DALRTDDN, 1, sizeof(DD_SYSTEM)-1, DD_SYSTEM };
    struct s99_common_text_unit stats = { DALSTATS, 1, 1, {DALSTATS_SHR} };

    rc = init_dsnam_text_unit(TEST_DSN, &dsn, NULL);
    if (rc) {
        record_fail(tname, "init_dsnam_text_unit failed");
        return;
    }

    rc = dsdd_alloc(&dsn, &dd, &stats, NULL);
    if (rc) {
        record_fail(tname, "dsdd_alloc failed - cannot set up double-free scenario");
        return;
    }

    /* First free - must succeed */
    rc = ddfree(&dd);
    if (rc != 0) {
        record_fail(tname, "first ddfree failed unexpectedly");
        return;
    }

    /*
     * Second free - SVC99 UNFREE should fail.
     * This call exercised the S0C4 path before the fix.
     * Execution reaching the next line proves no abend occurred.
     */
    rc = ddfree(&dd);
    if (rc == 0) {
        record_fail(tname, "second ddfree returned 0 - SVC99 UNFREE should have failed");
        return;
    }

    fprintf(stdout, "  [info] ddfree on freed DDname rc=%d - no abend\n", rc);
    record_pass(tname);
}

/* ------------------------------------------------------------------ */
/*
 * Test 3 - s99_prt_msg() with opts==NULL: guard in s99_prt_msg
 *
 * Calls s99_prt_msg() directly with opts==NULL.  Uses S99VRBUN so
 * emidnum→EMFREE, the same path taken by ddfree() on error.
 *
 * All SVC99 control blocks are allocated below the bar (MALLOC31) as
 * required for SVC99 / S99MSG.  The text-unit pointer array has the
 * high-bit set on its single entry to mark end-of-list.
 *
 * Assertion: s99_prt_msg returns without abending.
 * The return value may be 0 (S99MSG retrieved a message) or non-zero
 * (S99MSG failed but debug output suppressed by NULL guard) - both
 * outcomes are acceptable; the absence of abend is what is verified.
 */
static void test_s99_prt_msg_null_opts(void)
{
    const char *tname = "test_s99_prt_msg_null_opts";
    int rc;

    struct s99rb* PTR32              fake_rb;
    struct s99_rbx* PTR32            fake_rbx;
    struct s99_text_unit* PTR32 * PTR32 txtpp;

    fake_rb  = MALLOC31(sizeof(struct s99rb));
    fake_rbx = MALLOC31(sizeof(struct s99_rbx));
    txtpp    = MALLOC31(sizeof(struct s99_text_unit* PTR32));

    if (!fake_rb || !fake_rbx || !txtpp) {
        record_fail(tname, "MALLOC31 failed for synthetic s99rb");
        FREE31(fake_rb);
        FREE31(fake_rbx);
        FREE31(txtpp);
        return;
    }

    memset(fake_rb,  0, sizeof(struct s99rb));
    memset(fake_rbx, 0, sizeof(struct s99_rbx));

    fake_rb->s99rbln  = (unsigned char) sizeof(struct s99rb);
    fake_rb->s99verb  = S99VRBUN;      /* UNFREE → emidnum = EMFREE (51) */
    fake_rb->s99error = 0x0238;        /* plausible SVC99 error code      */
    fake_rb->s99txtpp = txtpp;
    fake_rb->s99s99x  = fake_rbx;

    /* Mark the single NULL slot as end of text-unit list */
    txtpp[0] = NULL;
    {
        unsigned int* PTR32 pp = (unsigned int* PTR32) &txtpp[0];
        *pp |= 0x80000000U;
    }

    /* RBX eye-catcher required by s99_fmt_dmp (DEBUG path only) */
    memcpy(fake_rbx->s99eid, "S99RBX", 6);
    fake_rbx->s99ever = S99RBXVR;

    /* Call under test: opts==NULL must not abend */
    rc = s99_prt_msg(NULL, fake_rb, 8);
    fprintf(stdout, "  [info] s99_prt_msg(NULL,...) rc=%d - no abend\n", rc);

    FREE31(txtpp);
    FREE31(fake_rbx);
    FREE31(fake_rb);

    record_pass(tname);
}

/* ------------------------------------------------------------------ */
int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    fprintf(stdout, "=== basicddfreetest: ddfree/s99_prt_msg NULL-opts S0C4 regression ===\n");

    test_alloc_free_roundtrip();
    test_ddfree_null_opts_on_error();
    test_s99_prt_msg_null_opts();

    fprintf(stdout, "=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
    return (g_failed > 0) ? 1 : 0;
}
