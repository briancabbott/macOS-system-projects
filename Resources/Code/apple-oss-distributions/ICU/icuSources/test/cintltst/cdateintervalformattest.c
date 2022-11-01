// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2011-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR DATE INTERVAL FORMAT */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/udateintervalformat.h"
#include "unicode/udat.h"
#include "unicode/ucal.h"
#include "unicode/ustring.h"
#include "unicode/udisplaycontext.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cformtst.h"

static void TestDateIntervalFormat(void);
static void TestFPos_SkelWithSeconds(void);
static void TestFormatToResult(void);
static void TestOpen(void);
static void TestFormatCalendarToResult(void);

void addDateIntervalFormatTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cdateintervalformattest/" #x)

void addDateIntervalFormatTest(TestNode** root)
{
    TESTCASE(TestDateIntervalFormat);
    TESTCASE(TestFPos_SkelWithSeconds);
    TESTCASE(TestFormatToResult);
    TESTCASE(TestOpen);
    TESTCASE(TestFormatCalendarToResult);
}

static const char tzUSPacific[] = "US/Pacific";
static const char tzAsiaTokyo[] = "Asia/Tokyo";
#define Date201103021030 1299090600000.0 /* 2011-Mar-02 Wed 1030 in US/Pacific, 2011-Mar-03 0330 in Asia/Tokyo */
#define Date201009270800 1285599629000.0 /* 2010-Sep-27 Mon 0800 in US/Pacific */
#define Date201712300900 1514653200000.0 /* 2017-Dec-30 Sat 0900 in US/Pacific */
#define Date200101012200 1546322400000.0 /* 2001-Jan-01 2200 in US/Pacific */
#define Date158210140000 -12219379142000.0
#define Date158210160000 -12219206342000.0
#define _MINUTE (60.0*1000.0)
#define _HOUR   (60.0*60.0*1000.0)
#define _DAY    (24.0*60.0*60.0*1000.0)

typedef struct {
    const char * locale;
    const char * skeleton;
    UDisplayContext context;
    UDateIntervalFormatAttributeValue minimizeType;
    const char * tzid;
    const UDate  from;
    const UDate  to;
    const char * resultExpected;
} DateIntervalFormatTestItem;

#define CAP_NONE  UDISPCTX_CAPITALIZATION_NONE
#define CAP_BEGIN UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE
#define CAP_LIST  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU
#define CAP_ALONE UDISPCTX_CAPITALIZATION_FOR_STANDALONE
#define CAP_MENU  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU
#define MIN_NONE   UDTITVFMT_MINIMIZE_NONE
#define MIN_MONTHS UDTITVFMT_MINIMIZE_ADJACENT_MONTHS
#define MIN_DAYS   UDTITVFMT_MINIMIZE_ADJACENT_DAYS

/* Just a small set of tests for now, the real functionality is tested in the C++ tests */
static const DateIntervalFormatTestItem testItems[] = {
    { "en",    "MMMdHHmm",  CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 7.0*_HOUR,  "Mar 2, 10:30\\u2009\\u2013\\u200917:30" },
    { "en",    "MMMdHHmm",  CAP_NONE,  MIN_NONE,   tzAsiaTokyo, Date201103021030,              Date201103021030 + 7.0*_HOUR,  "Mar 3, 03:30\\u2009\\u2013\\u200910:30" },
    { "en",    "yMMMEd",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 12.0*_HOUR, "Mon, Sep 27, 2010" },
    { "en",    "yMMMEd",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 31.0*_DAY,  "Mon, Sep 27\\u2009\\u2013\\u2009Thu, Oct 28, 2010" },
    { "en",    "yMMMEd",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 410.0*_DAY, "Mon, Sep 27, 2010\\u2009\\u2013\\u2009Fri, Nov 11, 2011" },
    { "de",    "Hm",        CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 12.0*_HOUR, "08:00\\u201320:00 Uhr" },
    { "de",    "Hm",        CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 31.0*_DAY,  "27.9.2010, 08:00\\u2009\\u2013\\u200928.10.2010, 08:00" },
    { "ja",    "MMMd",      CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 1.0*_DAY,   "9\\u670827\\u65E5\\uFF5E28\\u65E5" },

    { "cs",    "MMMEd",     CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "po 27.\\u202F9.\\u2009\\u2013\\u2009p\\u00E1 26.\\u202F11." },
    { "cs",    "yMMMM",     CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "z\\u00E1\\u0159\\u00ED\\u2013listopad 2010" },
    { "cs",    "yMMMM",     CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 1.0*_DAY,   "z\\u00E1\\u0159\\u00ED 2010" },
#if !UCONFIG_NO_BREAK_ITERATION
    { "cs",    "MMMEd",     CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "Po 27.\\u202F9.\\u2009\\u2013\\u2009p\\u00E1 26.\\u202F11." },
    { "cs",    "yMMMM",     CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "Z\\u00E1\\u0159\\u00ED\\u2013listopad 2010" },
    { "cs",    "yMMMM",     CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 1.0*_DAY,   "Z\\u00E1\\u0159\\u00ED 2010" },
    { "cs",    "MMMEd",     CAP_LIST,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "Po 27.\\u202F9.\\u2009\\u2013\\u2009p\\u00E1 26.\\u202F11." },
    { "cs",    "yMMMM",     CAP_LIST,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "Z\\u00E1\\u0159\\u00ED\\u2013listopad 2010" },
    { "cs",    "yMMMM",     CAP_LIST,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 1.0*_DAY,   "Z\\u00E1\\u0159\\u00ED 2010" },
    { "cs",    "MMMEd",     CAP_ALONE, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "po 27.\\u202F9.\\u2009\\u2013\\u2009p\\u00E1 26.\\u202F11." },
#endif
    { "cs",    "yMMMM",     CAP_ALONE, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 60.0*_DAY,  "z\\u00E1\\u0159\\u00ED\\u2013listopad 2010" },
    { "cs",    "yMMMM",     CAP_ALONE, MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 1.0*_DAY,   "z\\u00E1\\u0159\\u00ED 2010" },

    
    { "en",    "jm",        CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 1.0*_HOUR,  "10:30\\u2009\\u2013\\u200911:30 AM" },
    { "en",    "jm",        CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 12.0*_HOUR, "10:30 AM\\u2009\\u2013\\u200910:30 PM" },
    { "it",    "yMMMMd",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 15.0*_DAY,  "2\\u201317 marzo 2011" },
    { "en_SA", "MMMd",      CAP_NONE,  MIN_NONE,   tzUSPacific, Date201009270800,              Date201009270800 + 6.0*_DAY,   "18\\u2009\\u2013\\u200924 Shwl." },
    { "en@calendar=islamic-umalqura", "MMMd", CAP_NONE, MIN_NONE, tzUSPacific, Date201009270800, Date201009270800 + 6.0*_DAY,   "Shwl. 18\\u2009\\u2013\\u200924" },
    { "fr",    "E",         CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "mer.\\u2009\\u2013\\u2009sam." },
    { "fr",    "E",         CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "Mer.\\u2009\\u2013\\u2009sam." },
    { "fr",    "E",         CAP_MENU,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "mer.\\u2009\\u2013\\u2009sam." },
    { "fr",    "E",         CAP_ALONE, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "Mer.\\u2009\\u2013\\u2009sam." },
    { "fr",    "yMMMM",     CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "mars 2011" },
    { "fr",    "yMMMM",     CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "Mars 2011" },
    { "fr",    "yMMMM",     CAP_MENU,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "mars 2011" },
    { "fr",    "yMMMM",     CAP_ALONE, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 3.0*_DAY,   "Mars 2011" },
    { "fr",    "yMMMM",     CAP_NONE,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 40.0*_DAY,  "mars\\u2009\\u2013\\u2009avril 2011" },
    { "fr",    "yMMMM",     CAP_BEGIN, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 40.0*_DAY,  "Mars\\u2009\\u2013\\u2009avril 2011" },
    { "fr",    "yMMMM",     CAP_MENU,  MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 40.0*_DAY,  "mars\\u2009\\u2013\\u2009avril 2011" },
    { "fr",    "yMMMM",     CAP_ALONE, MIN_NONE,   tzUSPacific, Date201103021030,              Date201103021030 + 40.0*_DAY,  "Mars\\u2009\\u2013\\u2009avril 2011" },
    // Apple-specific
    { "en",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 6.0*_DAY,   "Sep 27\\u2009\\u2013\\u20093" },
    { "en",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 32.0*_DAY,  "Sep 27\\u2009\\u2013\\u2009Oct 29" },
    { "en",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 6.0*_DAY,   "Dec 30\\u2009\\u2013\\u20095" }, // across year boundary
    { "en",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 32.0*_DAY,  "Dec 30, 2017\\u2009\\u2013\\u2009Jan 31, 2018" }, // across year boundary but > 1 month
    { "fr",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 6.0*_DAY,   "27\\u20133 oct." },
    { "fr",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 32.0*_DAY,  "27 sept.\\u2009\\u2013\\u200929 oct." },
    { "fr",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 6.0*_DAY,   "30\\u20135 janv." }, // across year boundary
    { "fr",    "MMMd",      CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 32.0*_DAY,  "30 d\\u00E9c. 2017\\u2009\\u2013\\u200931 janv. 2018" }, // across year boundary but > 1 month

    { "en",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 6.0*_DAY,   "Sep 27\\u2009\\u2013\\u20093, 2010" },
    { "en",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 32.0*_DAY,  "Sep 27\\u2009\\u2013\\u2009Oct 29, 2010" },
    { "en",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 6.0*_DAY,   "Dec 30, 2017\\u2009\\u2013\\u2009Jan 5, 2018" }, // across year boundary
    { "en",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 32.0*_DAY,  "Dec 30, 2017\\u2009\\u2013\\u2009Jan 31, 2018" }, // across year boundary but > 1 month
    { "fr",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 6.0*_DAY,   "27\\u20133 oct. 2010" },
    { "fr",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201009270800,              Date201009270800 + 32.0*_DAY,  "27 sept.\\u2009\\u2013\\u200929 oct. 2010" },
    { "fr",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 6.0*_DAY,   "30 d\\u00E9c. 2017\\u2009\\u2013\\u20095 janv. 2018" }, // across year boundary
    { "fr",    "yMMMd",     CAP_NONE,  MIN_MONTHS, tzUSPacific, Date201712300900,              Date201712300900 + 32.0*_DAY,  "30 d\\u00E9c. 2017\\u2009\\u2013\\u200931 janv. 2018" }, // across year boundary but > 1 month

    { "en",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 10.0*_HOUR, "Sep 27, 8:00 AM\\u2009\\u2013\\u20096:00 PM" },
    { "en",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "Sep 27, 8:00 AM\\u2009\\u2013\\u2009Sep 28, 1:00 AM" },
    { "en",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "Sep 27, 8:00 PM\\u2009\\u2013\\u20091:00 AM" },
    { "en",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 26.0*_HOUR, "Sep 27, 8:00 PM\\u2009\\u2013\\u2009Sep 28, 10:00 AM" },
    { "en",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 35.0*_HOUR, "Sep 27, 8:00 PM\\u2009\\u2013\\u2009Sep 28, 7:00 PM" },
    { "fr",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 10.0*_HOUR, "27 sept. \\u00E0 08:00\\u2009\\u2013\\u200918:00" },
    { "fr",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "27 sept. \\u00E0 08:00\\u2009\\u2013\\u200928 sept. \\u00E0 01:00" },
    { "fr",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "27 sept. \\u00E0 20:00\\u2009\\u2013\\u200901:00" },
    { "fr",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 26.0*_HOUR, "27 sept. \\u00E0 20:00\\u2009\\u2013\\u200928 sept. \\u00E0 10:00" },
    { "fr",    "MMMdjmm",   CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 35.0*_HOUR, "27 sept. \\u00E0 20:00\\u2009\\u2013\\u200928 sept. \\u00E0 19:00" },

    // additional tests for rdar://problem/26911014 (spot check only for now)
    { "fr_US", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "27 septembre \\u00E0 8:00 AM\\u2009\\u2013\\u200928 septembre \\u00E0 1:00 AM" },
    { "fr_US", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "27 septembre \\u00E0 8:00 PM\\u2009\\u2013\\u20091:00 AM" },
    { "fr_AR", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "27 septembre \\u00e0 08:00\\u2009\\u2013\\u200928 septembre \\u00e0 01:00" },
    { "fr_AR", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "27 septembre \\u00e0 20:00\\u2009\\u2013\\u200901:00" },
    { "fr_JP", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "27 septembre \\u00e0 08:00\\u2009\\u2013\\u200928 septembre \\u00e0 01:00" },
    { "fr_JP", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "27 septembre \\u00e0 20:00\\u2009\\u2013\\u200901:00" },
    { "ja_US", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800,              Date201009270800 + 17.0*_HOUR, "9\\u670827\\u65e5 \\u5348\\u524d8:00\\uff5e9\\u670828\\u65e5 \\u5348\\u524d1:00" },
    { "ja_US", "MMMMdjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "9\\u670827\\u65e5 \\u5348\\u5f8c8\\u664200\\u5206\\uff5e\\u5348\\u524d1\\u664200\\u5206" },
    
    // additional tests for rdar://63628118 (spot check only for now)
    { "fr_DE", "yMEd",      CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201103021030,              Date201712300900,              "mer. 02.03.2011\\u2009\\u2013\\u2009sam. 30.12.2017" },
    { "de_FR", "yMEd",      CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201103021030,              Date201712300900,              "Mi. 02/03/2011\\u2009\\u2013\\u2009Sa. 30/12/2017" },
    { "fr_SA", "yMEd",      CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201103021030,              Date201712300900,              "mer.\\u060c 27/3/1432 \\u2013 sam.\\u060c 12/4/1439" },
    
    // tests for rdar://63926701 (the first test is the 'real' test; the second one never failed and is there to guard against regressions)
    { "fi_US", "yMdhmm",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date200101012200,              Date200101012200 + 1.5*_HOUR, "12/31/2018 klo 10.00\\u201311.30 ip." },
    { "fi_US", "yMdjmm",    CAP_NONE,  MIN_NONE,   tzUSPacific, Date200101012200,              Date200101012200 + 1.5*_HOUR, "12/31/2018 klo 10.00\\u201311.30 ip." },

    // rdar://55667608 and https://unicode-org.atlassian.net/browse/CLDR-10321
    { "fi",    "MMMdjjmm",  CAP_NONE,  MIN_DAYS,   tzUSPacific, Date201009270800 + 12.0*_HOUR, Date201009270800 + 17.0*_HOUR, "27.9. klo 20.00\\u20131.00" },
    
    // rdar://72744707
    { "hi@calendar=japanese", "yMMMEd", CAP_NONE, MIN_NONE, tzUSPacific, Date201009270800,     Date201009270800 + 31.0*_DAY,  "\\u0938\\u094B\\u092E, 27 \\u0938\\u093F\\u0924\\u0970 \\u2013 \\u0917\\u0941\\u0930\\u0941, 28 \\u0905\\u0915\\u094D\\u0924\\u0942\\u0970 22 \\u0939\\u0947\\u0907\\u0938\\u0947\\u0907" }, // "सोम, 27 सित॰ \\u2013 गुरु, 28 अक्तू॰ 22 हेइसेइ"
    { "hi@calendar=japanese", "yMMMEd", CAP_NONE, MIN_NONE, tzUSPacific, Date201009270800,     Date201009270800 + 410.0*_DAY, "\\u0938\\u094B\\u092E, 27 \\u0938\\u093F\\u0924\\u0970 22 \\u2013 \\u0936\\u0941\\u0915\\u094D\\u0930, 11 \\u0928\\u0935\\u0970 23 \\u0939\\u0947\\u0907\\u0938\\u0947\\u0907" }, // "सोम, 27 सित॰ 22 \\u2013 शुक्र, 11 नव॰ 23 हेइसेइ"

    { NULL,    NULL,        CAP_NONE,  MIN_NONE,   NULL,        0,                             0,                             NULL }
};

enum {
    kSkelBufLen = 32,
    kTZIDBufLen = 96,
    kFormatBufLen = 128
};

static void TestDateIntervalFormat()
{
    const DateIntervalFormatTestItem * testItemPtr;
    UErrorCode status = U_ZERO_ERROR;
    ctest_setTimeZone(NULL, &status);
    log_verbose("\nTesting udtitvfmt_open() and udtitvfmt_format() with various parameters\n");
    for ( testItemPtr = testItems; testItemPtr->locale != NULL; ++testItemPtr ) {
        UDateIntervalFormat* udtitvfmt;
        int32_t tzidLen;
        UChar skelBuf[kSkelBufLen];
        UChar tzidBuf[kTZIDBufLen];
        const char * tzidForLog = (testItemPtr->tzid)? testItemPtr->tzid: "NULL";

        status = U_ZERO_ERROR;
        u_unescape(testItemPtr->skeleton, skelBuf, kSkelBufLen);
        if ( testItemPtr->tzid ) {
            u_unescape(testItemPtr->tzid, tzidBuf, kTZIDBufLen);
            tzidLen = -1;
        } else {
            tzidLen = 0;
        }
        udtitvfmt = udtitvfmt_open(testItemPtr->locale, skelBuf, -1, tzidBuf, tzidLen, &status);
        if ( U_SUCCESS(status) ) {
            UChar result[kFormatBufLen];
            UChar resultExpected[kFormatBufLen];
            udtitvfmt_setAttribute(udtitvfmt, UDTITVFMT_MINIMIZE_TYPE, testItemPtr->minimizeType, &status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: udtitvfmt_setAttribute for locale %s, skeleton %s, tzid %s, minimizeType %d: %s\n",
                        testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->minimizeType, myErrorName(status) );
                continue;
            }
            udtitvfmt_setContext(udtitvfmt, testItemPtr->context, &status);
            if ( U_FAILURE(status) ) {
                log_err("FAIL: udtitvfmt_setContext for locale %s, skeleton %s, tzid %s, context %04X: %s\n",
                        testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->context, myErrorName(status) );
                continue;
            } else {
                UDisplayContext getContext = udtitvfmt_getContext(udtitvfmt, UDISPCTX_TYPE_CAPITALIZATION, &status);
                if ( U_FAILURE(status) ) {
                    log_err("FAIL: udtitvfmt_getContext for locale %s, skeleton %s, tzid %s, context %04X: %s\n",
                            testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->context, myErrorName(status) );
                    continue;
                } else if (getContext != testItemPtr->context) {
                    log_err("FAIL: udtitvfmt_getContext for locale %s, skeleton %s, tzid %s, set context %04X but got %04X\n",
                            testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->context, getContext );
                    continue;
                }
            }
            status = U_ZERO_ERROR;

            int32_t fmtLen = udtitvfmt_format(udtitvfmt, testItemPtr->from, testItemPtr->to, result, kFormatBufLen, NULL, &status);
            if (fmtLen >= kFormatBufLen) {
                result[kFormatBufLen-1] = 0;
            }
            if ( U_SUCCESS(status) ) {
                u_unescape(testItemPtr->resultExpected, resultExpected, kFormatBufLen);
                if ( u_strcmp(result, resultExpected) != 0 ) {
                    char bcharBuf[kFormatBufLen];
#if 0
                    log_err("ERROR: udtitvfmt_format for locale %s, skeleton %s, tzid %s, minimizeType %d, from %.1f, to %.1f: expect %s, get %s\n",
                             testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->minimizeType,
                             testItemPtr->from, testItemPtr->to, testItemPtr->resultExpected, u_austrcpy(bcharBuf,result) );
#else
                    // Apple-specific version
                    char bexpbuf[kFormatBufLen];
                    u_strToUTF8(bexpbuf, kFormatBufLen, NULL, resultExpected, -1, &status);
                    u_strToUTF8(bcharBuf, kFormatBufLen, NULL, result, fmtLen, &status);
                    log_err("ERROR: udtitvfmt_format for locale %s, skeleton %s, tzid %s, minimizeType %d, context %04X, from %.1f, to %.1f: expect %s, get %s\n",
                             testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->minimizeType, (int)testItemPtr->context,
                             testItemPtr->from, testItemPtr->to, bexpbuf, bcharBuf );
#endif
                }
            } else {
                log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, tzid %s, minimizeType %d, from %.1f, to %.1f: %s\n",
                        testItemPtr->locale, testItemPtr->skeleton, tzidForLog, (int)testItemPtr->minimizeType,
                        testItemPtr->from, testItemPtr->to, myErrorName(status) );
            }
            udtitvfmt_close(udtitvfmt);
        } else {
            log_data_err("FAIL: udtitvfmt_open for locale %s, skeleton %s, tzid %s - %s\n",
                    testItemPtr->locale, testItemPtr->skeleton, tzidForLog, myErrorName(status) );
        }
    }
    ctest_resetTimeZone();
}

/********************************************************************
 * TestFPos_SkelWithSeconds and related data
 ********************************************************************
 */

static UChar zoneGMT[] = { 0x47,0x4D,0x54,0 }; // GMT
static const UDate startTime = 1416474000000.0; // 2014 Nov 20 09:00 GMT

static const double deltas[] = {
	0.0, // none
	200.0, // 200 millisec
	20000.0, // 20 sec
	1200000.0, // 20 min
	7200000.0, // 2 hrs
	43200000.0, // 12 hrs
	691200000.0, // 8 days
	1382400000.0, // 16 days,
	8640000000.0, // 100 days
	-1.0
};
enum { kNumDeltas = UPRV_LENGTHOF(deltas) - 1 };

typedef struct {
    int32_t posBegin;
    int32_t posEnd;
    const char * format;
} ExpectPosAndFormat;

static const ExpectPosAndFormat exp_en_HHmm[kNumDeltas] = {
    {  3,  5, "09:00" },
    {  3,  5, "09:00" },
    {  3,  5, "09:00" },
    {  3,  5, "09:00\\u2009\\u2013\\u200909:20" },
    {  3,  5, "09:00\\u2009\\u2013\\u200911:00" },
    {  3,  5, "09:00\\u2009\\u2013\\u200921:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u200911/28/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u200912/6/2014, 09:00" },
    { 15, 17, "11/20/2014, 09:00\\u2009\\u2013\\u20092/28/2015, 09:00" }
};

static const ExpectPosAndFormat exp_en_HHmmss[kNumDeltas] = {
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200909:00:20" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200909:20:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200911:00:00" },
    {  3,  5, "09:00:00\\u2009\\u2013\\u200921:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u200911/28/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u200912/6/2014, 09:00:00" },
    { 15, 17, "11/20/2014, 09:00:00\\u2009\\u2013\\u20092/28/2015, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yyMMdd[kNumDeltas] = {
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u200911/28/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u200912/6/14" },
    {  0,  0, "11/20/14\\u2009\\u2013\\u20092/28/15" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmm[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200909:20" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200911:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200921:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200911/28/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200912/06/14, 09:00" },
    { 13, 15, "11/20/14, 09:00\\u2009\\u2013\\u200902/28/15, 09:00" }
};

static const ExpectPosAndFormat exp_en_yyMMddHHmmss[kNumDeltas] = {
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200909:00:20" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200909:20:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200911:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200921:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200911/28/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200912/06/14, 09:00:00" },
    { 13, 15, "11/20/14, 09:00:00\\u2009\\u2013\\u200902/28/15, 09:00:00" }
};

static const ExpectPosAndFormat exp_en_yMMMdhmmssz[kNumDeltas] = {
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u20099:00:20 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u20099:20:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u200911:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u20099:00:00 PM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u2009Nov 28, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u2009Dec 6, 2014, 9:00:00 AM GMT" },
    { 16, 18, "Nov 20, 2014, 9:00:00 AM GMT\\u2009\\u2013\\u2009Feb 28, 2015, 9:00:00 AM GMT" }
};

static const ExpectPosAndFormat exp_ja_yyMMddHHmm[kNumDeltas] = {
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9:00" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E9\\u664220\\u5206" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E11\\u664200\\u5206" },
    { 11, 13, "14/11/20 9\\u664200\\u5206\\uFF5E21\\u664200\\u5206" },
    { 11, 13, "14/11/20 9:00\\uFF5E14/11/28 9:00" },
    { 11, 13, "14/11/20 9:00\\uFF5E14/12/06 9:00" },
    { 11, 13, "14/11/20 9:00\\uFF5E15/02/28 9:00" }
};

static const ExpectPosAndFormat exp_ja_yyMMddHHmmss[kNumDeltas] = {
    { 11, 13, "14/11/20 9:00:00" },
    { 11, 13, "14/11/20 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E9:00:20" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E9:20:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E11:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E21:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E14/11/28 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E14/12/06 9:00:00" },
    { 11, 13, "14/11/20 9:00:00\\uFF5E15/02/28 9:00:00" }
};

static const ExpectPosAndFormat exp_ja_yMMMdHHmmss[kNumDeltas] = {
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E9:00:20" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E9:20:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E11:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E21:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2014\\u5E7411\\u670828\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2014\\u5E7412\\u67086\\u65E5 9:00:00" },
    { 14, 16, "2014\\u5E7411\\u670820\\u65E5 9:00:00\\uFF5E2015\\u5E742\\u670828\\u65E5 9:00:00" }
};

// rdar://55667608 and https://unicode-org.atlassian.net/browse/CLDR-10321
static const ExpectPosAndFormat exp_fi_MMMdjjmm[kNumDeltas] = {
    { 13, 15, "20.11. klo 9.00" },
    { 13, 15, "20.11. klo 9.00" },
    { 13, 15, "20.11. klo 9.00" },
    { 13, 15, "20.11. klo 9.00\\u20139.20" },
    { 13, 15, "20.11. klo 9.00\\u201311.00" },
    { 13, 15, "20.11. klo 9.00\\u201321.00" },
    { 13, 15, "20.11. klo 9.00\\u201328.11. klo 9.00" },
    { 13, 15, "20.11. klo 9.00\\u20136.12. klo 9.00" },
    { 17, 19, "20.11.2014 klo 9.00\\u201328.2.2015 klo 9.00" }
};

typedef struct {
	const char * locale;
	const char * skeleton;
	UDateFormatField fieldToCheck;
	const ExpectPosAndFormat * expected;
} LocaleAndSkeletonItem;

static const LocaleAndSkeletonItem locSkelItems[] = {
	{ "en",		"HHmm",         UDAT_MINUTE_FIELD, exp_en_HHmm },
	{ "en",		"HHmmss",       UDAT_MINUTE_FIELD, exp_en_HHmmss },
	{ "en",		"yyMMdd",       UDAT_MINUTE_FIELD, exp_en_yyMMdd },
	{ "en",		"yyMMddHHmm",   UDAT_MINUTE_FIELD, exp_en_yyMMddHHmm },
	{ "en",		"yyMMddHHmmss", UDAT_MINUTE_FIELD, exp_en_yyMMddHHmmss },
	{ "en",		"yMMMdhmmssz",  UDAT_MINUTE_FIELD, exp_en_yMMMdhmmssz },
	{ "ja",		"yyMMddHHmm",   UDAT_MINUTE_FIELD, exp_ja_yyMMddHHmm },
	{ "ja",		"yyMMddHHmmss", UDAT_MINUTE_FIELD, exp_ja_yyMMddHHmmss },
	{ "ja",		"yMMMdHHmmss",  UDAT_MINUTE_FIELD, exp_ja_yMMMdHHmmss },
    { "fi",     "MMMdjjmm",     UDAT_MINUTE_FIELD, exp_fi_MMMdjjmm },
	{ NULL, NULL, (UDateFormatField)0, NULL }
};

enum { kSizeUBuf = 96, kSizeBBuf = 192 };

static void TestFPos_SkelWithSeconds()
{
	const LocaleAndSkeletonItem * locSkelItemPtr;
	for (locSkelItemPtr = locSkelItems; locSkelItemPtr->locale != NULL; locSkelItemPtr++) {
	    UDateIntervalFormat* udifmt;
	    UChar   ubuf[kSizeUBuf];
	    int32_t ulen, uelen;
	    UErrorCode status = U_ZERO_ERROR;
            ulen = u_unescape(locSkelItemPtr->skeleton, ubuf, kSizeUBuf);
	    udifmt = udtitvfmt_open(locSkelItemPtr->locale, ubuf, ulen, zoneGMT, -1, &status);
	    if ( U_FAILURE(status) ) {
           log_data_err("FAIL: udtitvfmt_open for locale %s, skeleton %s: %s\n",
                    locSkelItemPtr->locale, locSkelItemPtr->skeleton, u_errorName(status));
	    } else {
			const double * deltasPtr = deltas;
			const ExpectPosAndFormat * expectedPtr = locSkelItemPtr->expected;
			for (; *deltasPtr >= 0.0; deltasPtr++, expectedPtr++) {
			    UFieldPosition fpos = { locSkelItemPtr->fieldToCheck, 0, 0 };
			    UChar uebuf[kSizeUBuf];
			    char bbuf[kSizeBBuf];
			    char bebuf[kSizeBBuf];
			    status = U_ZERO_ERROR;
			    uelen = u_unescape(expectedPtr->format, uebuf, kSizeUBuf);
			    ulen = udtitvfmt_format(udifmt, startTime, startTime + *deltasPtr, ubuf, kSizeUBuf, &fpos, &status);
			    if ( U_FAILURE(status) ) {
			        log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, delta %.1f: %s\n",
			             locSkelItemPtr->locale, locSkelItemPtr->skeleton, *deltasPtr, u_errorName(status));
			    } else if ( ulen != uelen || u_strncmp(ubuf,uebuf,uelen) != 0 ||
			                fpos.beginIndex != expectedPtr->posBegin || fpos.endIndex != expectedPtr->posEnd ) {
			        u_strToUTF8(bbuf, kSizeBBuf, NULL, ubuf, ulen, &status);
			        u_strToUTF8(bebuf, kSizeBBuf, NULL, uebuf, uelen, &status); // convert back to get unescaped string
			        log_err("FAIL: udtitvfmt_format for locale %s, skeleton %s, delta %12.1f, expect %d-%d \"%s\", get %d-%d \"%s\"\n",
			             locSkelItemPtr->locale, locSkelItemPtr->skeleton, *deltasPtr,
			             expectedPtr->posBegin, expectedPtr->posEnd, bebuf,
			             fpos.beginIndex, fpos.endIndex, bbuf);
			    }
			}
	        udtitvfmt_close(udifmt);
	    }
    }
}

static void TestFormatToResult() {
    UErrorCode ec = U_ZERO_ERROR;
    UDateIntervalFormat* fmt = udtitvfmt_open("de", u"dMMMMyHHmm", -1, zoneGMT, -1, &ec);
    UFormattedDateInterval* fdi = udtitvfmt_openResult(&ec);
    assertSuccess("Opening", &ec);

    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010, 15:00\u2009–\u20092. März 2011, 18:30";
        udtitvfmt_formatToResult(fmt, Date201009270800, Date201103021030, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 25},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 28, 47},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 28, 29},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 31, 35},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 36, 40},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 42, 44},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 45, 47}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 2";
        const UChar* expectedString = u"27. September 2010, 15:00–22:00 Uhr";
        udtitvfmt_formatToResult(fmt, Date201009270800, Date201009270800 + 7*_HOUR, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 20, 25},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 26, 31},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 26, 28},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 29, 31},
            {UFIELD_CATEGORY_DATE, UDAT_AM_PM_FIELD, 32, 35}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    udtitvfmt_close(fmt);
    udtitvfmt_closeResult(fdi);
}

static void TestFormatCalendarToResult() {
    UErrorCode ec = U_ZERO_ERROR;
    UCalendar* ucal1 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal1, Date201009270800, &ec);
    UCalendar* ucal2 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal2, Date201103021030, &ec);
    UCalendar* ucal3 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    ucal_setMillis(ucal3, Date201009270800 + 7*_HOUR, &ec);
    UCalendar* ucal4 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);
    UCalendar* ucal5 = ucal_open(zoneGMT, -1, "de", UCAL_DEFAULT, &ec);

    UDateIntervalFormat* fmt = udtitvfmt_open("de", u"dMMMMyHHmm", -1, zoneGMT, -1, &ec);
    UFormattedDateInterval* fdi = udtitvfmt_openResult(&ec);
    assertSuccess("Opening", &ec);

    {
        const char* message = "Field position test 1";
        const UChar* expectedString = u"27. September 2010, 15:00 – 2. März 2011, 18:30";
        udtitvfmt_formatCalendarToResult(fmt, ucal1, ucal2, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 25},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 28, 47},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 28, 29},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 31, 35},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 36, 40},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 42, 44},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 45, 47}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 2";
        const UChar* expectedString = u"27. September 2010, 15:00–22:00 Uhr";
        udtitvfmt_formatCalendarToResult(fmt, ucal1, ucal3, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 13},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 20, 25},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 23, 25},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 26, 31},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 26, 28},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 29, 31},
            {UFIELD_CATEGORY_DATE, UDAT_AM_PM_FIELD, 32, 35}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        const char* message = "Field position test 3";
        // Date across Julian Gregorian change date.
        ucal_setMillis(ucal4, Date158210140000, &ec);
        ucal_setMillis(ucal5, Date158210160000, &ec);
        //                                        1         2         3         4
        //                              012345678901234567890123456789012345678901234567890
        const UChar* expectedString = u"4. Oktober 1582, 00:00 – 16. Oktober 1582, 00:00";
        udtitvfmt_formatCalendarToResult(fmt, ucal4, ucal5, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 22},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 1},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 3, 10},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 11, 15},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 17, 19},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 20, 22},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 25, 48},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 25, 27},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 29, 36},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 37, 41},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 43, 45},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 46, 48}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
    {
        // Date across Julian Gregorian change date.
        // We set the Gregorian Change way back.
        ucal_setGregorianChange(ucal5, (UDate)(-8.64e15), &ec);
        ucal_setGregorianChange(ucal4, (UDate)(-8.64e15), &ec);
        ucal_setMillis(ucal4, Date158210140000, &ec);
        ucal_setMillis(ucal5, Date158210160000, &ec);
        const char* message = "Field position test 4";
        //                                        1         2         3         4
        //                              012345678901234567890123456789012345678901234567890
        const UChar* expectedString = u"14. Oktober 1582, 00:00 – 16. Oktober 1582, 00:00";
        udtitvfmt_formatCalendarToResult(fmt, ucal4, ucal5, fdi, &ec);
        assertSuccess("Formatting", &ec);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // category, field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 0, 23},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 0, 2},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 4, 11},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 12, 16},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 18, 20},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 21, 23},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 26, 49},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 26, 28},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 30, 37},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 38, 42},
            {UFIELD_CATEGORY_DATE, UDAT_HOUR_OF_DAY0_FIELD, 44, 46},
            {UFIELD_CATEGORY_DATE, UDAT_MINUTE_FIELD, 47, 49}};
        checkMixedFormattedValue(
            message,
            udtitvfmt_resultAsValue(fdi, &ec),
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    ucal_close(ucal1);
    ucal_close(ucal2);
    ucal_close(ucal3);
    ucal_close(ucal4);
    ucal_close(ucal5);
    udtitvfmt_close(fmt);
    udtitvfmt_closeResult(fdi);
}

static const char* openLocales[] = {
    "en",
    "en@calendar=japanese",
    "en@calendar=coptic",
    "en@calendar=chinese",
    "en_001",
    "en_001@calendar=japanese",
    "en_001@calendar=coptic",
    "en_001@calendar=chinese",
    "en_AU",
    "en_AU@calendar=japanese", // had problems
    "en_AU@calendar=coptic", // had problems
    "en_AU@calendar=chinese",
    "en_CA",
    "en_CA@calendar=japanese", // had problems
    "en_CA@calendar=coptic", // had problems
    "en_CA@calendar=chinese",
    "en_CN",
    "en_CN@calendar=japanese", // had problems
    "en_CN@calendar=coptic", // had problems
    "en_CN@calendar=chinese",
    "en_DE@calendar=japanese", // had problems
    "en_DE@calendar=coptic", // had problems
    "en_GB",
    "en_GB@calendar=japanese", // had problems
    "en_GB@calendar=coptic", // had problems
    "en_GB@calendar=chinese",
    "en_HK@calendar=japanese", // had problems
    "en_HK@calendar=coptic", // had problems
    "en_IE@calendar=japanese", // had problems
    "en_IE@calendar=coptic", // had problems
    "en_IN@calendar=japanese", // had problems
    "en_IN@calendar=coptic", // had problems
    "en_JP",
    "en_JP@calendar=japanese",
    "en_JP@calendar=coptic",
    "en_JP@calendar=chinese",
    "en_NZ",
    "en_NZ@calendar=japanese", // had problems
    "en_NZ@calendar=coptic", // had problems
    "en_NZ@calendar=chinese",
    "en_SG@calendar=japanese", // had problems
    "en_SG@calendar=coptic", // had problems
    "es",
    "es@calendar=japanese",
    "es@calendar=coptic",
    "es@calendar=chinese",
    "es_419",
    "es_419@calendar=japanese", // had problems
    "es_419@calendar=coptic", // had problems
    "es_419@calendar=chinese",
    "es_MX",
    "es_MX@calendar=japanese",
    "es_MX@calendar=coptic",
    "es_MX@calendar=chinese",
    "es_US",
    "es_US@calendar=japanese",
    "es_US@calendar=coptic",
    "es_US@calendar=chinese",
    "fr",
    "fr@calendar=japanese",
    "fr@calendar=coptic",
    "fr@calendar=chinese",
    "fr_CA",
    "fr_CA@calendar=japanese", // had problems
    "fr_CA@calendar=coptic", // had problems
    "fr_CA@calendar=chinese",
    "fr_CH",
    "fr_CH@calendar=japanese",
    "fr_CH@calendar=coptic",
    "fr_CH@calendar=chinese",
    "fr_BE",
    "fr_BE@calendar=japanese",
    "fr_BE@calendar=coptic",
    "fr_BE@calendar=chinese",
    "nl_BE@calendar=japanese", // had problems
    "nl_BE@calendar=coptic", // had problems
    "pt",
    "pt@calendar=japanese",
    "pt@calendar=coptic",
    "pt@calendar=chinese",
    "pt_PT",
    "pt_PT@calendar=japanese", // had problems
    "pt_PT@calendar=coptic", // had problems
    "pt_PT@calendar=chinese",
    "zh_Hant",
    "zh_Hant@calendar=japanese",
    "zh_Hant@calendar=coptic",
    "zh_Hant@calendar=chinese",
    "zh_Hant_HK",
    "zh_Hant_HK@calendar=japanese", // had problems
    "zh_Hant_HK@calendar=coptic", // had problems
    "zh_Hant_HK@calendar=chinese",
    NULL
};
static const UChar* openSkeleton = u"zzzzyMMMMEEEEdhmmss";
static const UChar* openZone = u"America/Vancouver";

static void TestOpen()
{
    const char* locale;
    const char** localesPtr = openLocales;
    while ((locale = *localesPtr++) != NULL) {
        UErrorCode status = U_ZERO_ERROR;
        UDateIntervalFormat* udatintv = udtitvfmt_open(locale, openSkeleton, -1, openZone, -1, &status);
        if ( U_FAILURE(status) ) {
            log_err("FAIL: udtitvfmt_open for locale %s: %s\n", locale, u_errorName(status));
        } else {
            udtitvfmt_close(udatintv);
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
