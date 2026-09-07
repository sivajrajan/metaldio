#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include "metaldio.h"
#include "dio.h"
#include "mem.h"
#include "s99.h"
#include "wrappers.h"
#include "msg.h"

static size_t text_unit_size(struct s99_text_unit* inunit) 
{
	struct s99_basic_text_unit* tunit = (struct s99_basic_text_unit*) inunit;
	size_t tunitsize;
	size_t i;
	switch (tunit->s99tukey) {
		case DALBRTKN:
			tunitsize = sizeof(struct s99_browse_token_text_unit); 
			break;
		default:
			tunitsize = sizeof(struct s99_basic_text_unit);
			for (i=0; i<tunit->s99tunum; ++i) {
				tunitsize += (sizeof(unsigned short) + tunit->entry[i].s99tulng);
			}
			break;
	}
	return tunitsize;
}

static struct s99_text_unit* PTR32 calloc_text_unit(struct s99_text_unit* inunit) 
{
	struct s99_text_unit* PTR32 outunit;
	int i;

	size_t tunitsize;
	tunitsize = text_unit_size(inunit);
	outunit = MALLOC31(tunitsize);
	if (outunit) {
		memcpy(outunit, inunit, tunitsize);
	}
	return outunit;
}

void s99_fmt_dmp(const DBG_Opts* opts, struct s99rb* PTR32 parms) 
{
	size_t tunitsize;
	unsigned int* PTR32 p;
	unsigned int* PTR32 pp;
	struct s99_text_unit* wtu;
	struct s99_text_unit* PTR32 * PTR32 textunit = parms->s99txtpp;
	struct s99_rbx* PTR32 rbx = parms->s99s99x;
	int i=0;
	char* s99verb = (char*)&parms->s99verb;
	unsigned short* s99flag1 = (unsigned short*)&parms->s99flag1;
	unsigned short* s99error = (unsigned short*)&parms->s99error;
	unsigned short* s99info = (unsigned short*)&parms->s99info;
	unsigned int* s99flag2 = (unsigned int*)&parms->s99flag2;

	errmsg(opts, "SVC99 Formatted Dump\n");
	errmsg(opts, "  RBLN:%d VERB:%d FLAG1:%4.4X ERROR:%4.4X INFO:%4.4X FLAG2:%8.8X\n", 
		parms->s99rbln, *s99verb, *s99flag1, *s99error, *s99info, *s99flag2);

	errmsg(opts, "SVC99 RB\n");
  dumpstg(opts, parms, sizeof(struct s99rb));


	if (rbx) {
		char* s99eopts = (char*) &rbx->s99eopts;
		char* s99emgsv = (char*) &rbx->s99emgsv;
	  errmsg(opts, "\nSVC99 RBX: %8.8X", rbx);
    dumpstg(opts, rbx, sizeof(struct s99_rbx));
		errmsg(opts, "\n  EID:%6.6s EVER: %2.2X EOPTS: %2.2X SUBP: %2.2x EKEY: %2.2X EMGSV: %2.2X ECPPL: %8.8X EMSGP: %8.8X ERCO: %2.2x\n", 
			rbx->s99eid, rbx->s99ever, *s99eopts, rbx->s99esubp, rbx->s99ekey, *s99emgsv, rbx->s99ecppl, rbx->s99emsgp, rbx->s99erco); 
	} else {
		errmsg(opts, "\n");
	}
	do {
		pp = (unsigned int* PTR32) &textunit[i];
		wtu = (struct s99_text_unit*) textunit[i];
		tunitsize = text_unit_size(wtu);
		errmsg(opts, "  textunit[%d] %X %3zu ", i, *pp, tunitsize);
		dumpstg(opts, textunit[i], tunitsize);
		errmsg(opts, "\n");
		++i;
	} while (((*pp) & 0x80000000) == 0);
	return;
}

struct s99rb* PTR32 s99_init(enum s99_verb verb, struct s99_flag1 flag1, struct s99_flag2 flag2, struct s99_rbx* rbxin, size_t num_text_units, ...)
{
	va_list arg_ptr;
	size_t i;
	struct s99rb* PTR32 parms;
	struct s99_rbx* PTR32 rbxp;
	struct s99_text_unit* PTR32 * PTR32 textunit;
	unsigned int* PTR32 pp;

	textunit = MALLOC31(num_text_units * (sizeof(struct s99_text_unit* PTR32)));
	if (!textunit) {
		return 0;
	}
	rbxp = MALLOC31(sizeof(struct s99_rbx));
	if (!rbxp) {
		return 0;
	} 
	parms = MALLOC31(sizeof(struct s99rb));
	if (!parms) {
		return 0;
	} 

	va_start(arg_ptr, num_text_units);
	for (i=0; i<num_text_units; ++i) {
		struct s99_text_unit* inunit = (struct s99_text_unit*) va_arg(arg_ptr, void*);
		textunit[i] = calloc_text_unit(inunit);
	}
	pp = (unsigned int* PTR32) (&textunit[num_text_units-1]);	
	*pp |= 0x80000000;

	va_end(arg_ptr);

	/* DIAG: print rbxin (64-bit incoming ptr from caller stack) and rbxp (PTR32 below-bar heap ptr).
	 * rbxin > 0x7FFFFFFF → above-bar 64-bit address → struct assign crosses the bar.
	 * rbxp  < 0x80000000 → below-bar 32-bit address → SVC99-safe destination.
	 * sizeof printed from both sides confirms whether pack(1)+PTR32 size agrees across the bar. */
	debug(opts, "s99_init ptrs: rbxin=0x%016llX sizeof(*rbxin)=%zu  rbxp=0x%08X sizeof(*rbxp)=%zu\n",
		(unsigned long long)(uintptr_t)rbxin, sizeof(*rbxin),
		(unsigned int)(uintptr_t)rbxp,        sizeof(*rbxp));

	*rbxp = *rbxin; /* struct-assign: crosses 64→31 bar when rbxin is a stack ptr; see AMODE64 ptrs diagnostic above */

	/* DIAG: print s99eid[0] of both source and destination after the assign.
	 * rbxin->s99eid[0] must be 0xE2 (EBCDIC 'S').  If rbxp->s99eid[0] is 0x00
	 * the cross-bar struct copy is confirmed as the corruption source.         */
	debug(opts, "s99_init post-assign: rbxin->s99eid[0]=0x%02X  rbxp->s99eid[0]=0x%02X (both must be 0xE2)\n",
		(unsigned char)rbxin->s99eid[0],
		(unsigned char)rbxp->s99eid[0]);

	parms->s99rbln = sizeof(struct s99rb);
	parms->s99verb = (unsigned char)verb; /* explicit cast: enum is 4 bytes, struct field is 1 byte; verb values 1-7 always fit */
	parms->s99flag1 = flag1;
	parms->s99txtpp = textunit;
	parms->s99s99x = rbxp;
	parms->s99flag2 = flag2;

	return parms;
}

void s99_free(struct s99rb* PTR32 parms) 
{
	int i=0;
	unsigned int txtunit;
	do {
		unsigned int* PTR32 txtunitp = (unsigned int* PTR32) (&parms->s99txtpp[i]);
		txtunit = *txtunitp;
		free(parms->s99txtpp[i]); 
		++i;
	} while ((txtunit & 0x80000000) == 0);
	free(parms->s99txtpp);
	free(parms->s99s99x);
	free(parms);
}

void s99_em_fmt_dmp(const DBG_Opts* opts, struct s99_em* PTR32 parms) {
	char* funct = (char* ) parms;
	errmsg(opts, "SVC99 EM Parms Dump\n");
	errmsg(opts, "  EMParms %8.8X FUNCT:%2.2X IDNUM:%2.2X NMSGBAK:%d S99RBP:%8.8X RETCOD:%8.8X CPPLP:%8.8X BUFP:%8.8X WTPCDP:%8.8X\n",
		funct, *funct, parms->emidnum, parms->emnmsgbk, parms->ems99rbp, parms->emretcod, parms->emcpplp, parms->embufp, parms->emwtpcdp);
}

/* s99_rbx_fmt_dmp - print every field of the s99_rbx by name so a failing IEFDB476 call can be fully diagnosed without a hex dump. */
static void s99_rbx_fmt_dmp(const DBG_Opts* opts, struct s99_rbx* PTR32 rbx) {
	if (!rbx) { errmsg(opts, "  S99RBX: <NULL>\n"); return; }
	unsigned char eopts_byte = *((unsigned char*)&rbx->s99eopts);   /* collapse bitfield to one byte for printing */
	unsigned char emgsv_byte = *((unsigned char*)&rbx->s99emgsv);   /* collapse bitfield to one byte for printing */
	errmsg(opts, "S99RBX Fields:\n");
	errmsg(opts, "  EID:    '%.6s'\n",  rbx->s99eid);               /* eye-catcher: must be 'S99RBX' */
	errmsg(opts, "  EVER:   0x%02X\n",  (unsigned char)rbx->s99ever);   /* version: must be 1 (S99RBXVR) */
	errmsg(opts, "  EOPTS:  0x%02X  (eimsg=%d ermsg=%d elsto=%d emkey=%d emsub=%d ewtp=%d)\n",
		eopts_byte,
		rbx->s99eopts.s99eimsg, rbx->s99eopts.s99ermsg,             /* ermsg=1 + emsgp=NULL → IEFDB476 rc=0x0C */
		rbx->s99eopts.s99elsto, rbx->s99eopts.s99emkey,
		rbx->s99eopts.s99emsub, rbx->s99eopts.s99ewtp);
	errmsg(opts, "  ESUBP:  0x%02X\n",  (unsigned char)rbx->s99esubp);  /* subsystem identifier (0=default) */
	errmsg(opts, "  EKEY:   0x%02X\n",  (unsigned char)rbx->s99ekey);   /* storage protect key (0=caller's key) */
	errmsg(opts, "  EMGSV:  0x%02X  (xseve=%d xwarn=%d)\n",
		emgsv_byte,
		rbx->s99emgsv.s99xseve, rbx->s99emgsv.s99xwarn);            /* message severity overrides */
	errmsg(opts, "  ENMSG:  0x%02X\n",  (unsigned char)rbx->s99enmsg);  /* number of messages to suppress */
	errmsg(opts, "  ECPPL:  0x%08X\n", rbx->s99ecppl);               /* CPPL pointer (0=not a TSO command) */
	errmsg(opts, "  ERCR:   0x%02X\n",  (unsigned char)rbx->s99ercr);   /* return code reason: routing */
	errmsg(opts, "  ERCM:   0x%02X\n",  (unsigned char)rbx->s99ercm);   /* return code reason: module */
	errmsg(opts, "  ERCO:   0x%02X\n",  (unsigned char)rbx->s99erco);   /* return code reason: offset */
	errmsg(opts, "  ERCF:   0x%02X\n",  (unsigned char)rbx->s99ercf);   /* return code reason: flag */
	errmsg(opts, "  EWRC:   0x%08X\n", rbx->s99ewrc);                /* extended return code */
	errmsg(opts, "  EMSGP:  0x%08X\n", rbx->s99emsgp);               /* message buffer ptr: must be non-NULL if ermsg=1 */
	errmsg(opts, "  EERR:   0x%04X\n",  rbx->s99eerr);                /* extended error code */
	errmsg(opts, "  EINFO:  0x%04X\n",  rbx->s99einfo);               /* extended info code */
	errmsg(opts, "  ERSN:   0x%08X\n", rbx->s99ersn);                /* extended reason code */
}

int s99_prt_msg(const DBG_Opts* opts, struct s99rb* PTR32 svc99parms, int svc99rc) 
{
	struct s99_em* PTR32 msgparms; 
	int rc;

	msgparms = MALLOC31(sizeof(struct s99_em));
	if (!msgparms) {
		return 16;
	}
	memset(msgparms, 0, sizeof(struct s99_em));
	msgparms->emreturn = 1;
	msgparms->emidnum = (svc99parms->s99verb == S99VRBUN) ? EMFREE : EMSVC99;
	msgparms->emnmsgbk = 2;
	msgparms->emretcod = svc99rc;
	msgparms->ems99rbp = svc99parms;
	msgparms->emwtpcdp = &msgparms->emwtdert;
	msgparms->embufp = &msgparms->embuf;

	/* Always dump raw s99error/s99info before calling IEFDB476; these are the definitive SVC99 reason codes even if S99MSG itself fails. */
	errmsg(opts, "SVC99 verb:0x%x rc:0x%x error:0x%04x info:0x%04x\n",
		(unsigned int)svc99parms->s99verb, svc99rc,
		(unsigned short)svc99parms->s99error, (unsigned short)svc99parms->s99info);

	rc = S99MSG(msgparms);
	if (rc) {
		errmsg(opts, "SVC99MSG rc:0x%x\n", rc);
		errmsg(opts, "IEFDB476 failed with rc:0x%x\n", rc);
		/* Dump the full RBX on any IEFDB476 failure — not just debug — so the exact bad field is always visible. */
		s99_rbx_fmt_dmp(opts, svc99parms->s99s99x);
		if (opts && opts->debug) {
			s99_em_fmt_dmp(opts, msgparms);
		}
	} else {
		info(opts, "%.*s\n", msgparms->embuf.embufl1, &msgparms->embuf.embuft1[msgparms->embuf.embufo1]);
		info(opts, "%.*s\n", msgparms->embuf.embufl2, &msgparms->embuf.embuft2[msgparms->embuf.embufo2]);
	}

	free(msgparms);

	return rc;
}
