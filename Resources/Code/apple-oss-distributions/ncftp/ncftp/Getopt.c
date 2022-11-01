/* Getopt.c */

#include "Sys.h"
#include "Util.h"
#include "Getopt.h"

int gOptErr = 1;					/* if error message should be printed */
int gOptInd = 1;					/* index into parent argv vector */
int gOptOpt;						/* character checked for validity */
char *gOptArg;						/* argument associated with option */
char *gOptPlace = kGetoptErrMsg;	/* saved position in an arg */

/* This must be called before each Getopt. */
void GetoptReset(void)
{
	gOptInd = 1;
	gOptPlace = kGetoptErrMsg;
}	/* GetoptReset */




static
char *NextOption(char *ostr)
{
	if ((gOptOpt = (int) *gOptPlace++) == (int) ':')
		return 0;
	return strchr(ostr, gOptOpt);
}	/* NextOption */




int Getopt(int nargc, char **nargv, char *ostr)
{
	register char *oli;				   /* Option letter list index */

	if (!*gOptPlace) {					   /* update scanning pointer */
		if (gOptInd >= nargc || *(gOptPlace = nargv[gOptInd]) != '-')
			return (EOF);
		if (gOptPlace[1] && *++gOptPlace == '-') {	/* found "--" */
			++gOptInd;
			return (EOF);
		}
	}								   /* Option letter okay? */
	oli = NextOption(ostr);
	if (oli == NULL) {
		if (!*gOptPlace)
			++gOptInd;
		if (gOptErr)
			PrintF("%s%s%c\n", *nargv, ": illegal option -- ", gOptOpt);
		return(kGetoptBadChar);
	}
	if (*++oli != ':') {			   /* don't need argument */
		gOptArg = NULL;
		if (!*gOptPlace)
			++gOptInd;
	} else {						   /* need an argument */
		if (*gOptPlace)					   /* no white space */
			gOptArg = gOptPlace;
		else if (nargc <= ++gOptInd) {  /* no arg */
			gOptPlace = kGetoptErrMsg;
			if (gOptErr) 
				PrintF("%s%s%c\n", *nargv, ": option requires an argument -- ", gOptOpt);
			return(kGetoptBadChar);
		} else						   /* white space */
			gOptArg = nargv[gOptInd];
		gOptPlace = kGetoptErrMsg;
		++gOptInd;
	}
	return (gOptOpt);				   /* dump back Option letter */
}									   /* Getopt */

/* eof */
