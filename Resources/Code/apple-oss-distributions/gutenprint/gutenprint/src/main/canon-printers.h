/*
 *   Print plug-in CANON BJL driver for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and
 *      Andy Thaller (thaller@ph.tum.de)
 *   Copyright (c) 2006 Sascha Sommer (saschasommer@freenet.de)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* This file contains the capabilities of the various canon printers
*/

#ifndef GUTENPRINT_INTERNAL_CANON_PRINTERS_H
#define GUTENPRINT_INTERNAL_CANON_PRINTERS_H


typedef struct canon_caps {
  int model;          /* model number as used in printers.xml */
  int model_id;       /* model ID code for use in commands */
  int max_width;      /* maximum printable paper size */
  int max_height;
  int border_left;    /* left margin, points */
  int border_right;   /* right margin, points */
  int border_top;     /* absolute top margin, points */
  int border_bottom;  /* absolute bottom margin, points */
  const canon_slotlist_t* slotlist; /*available paperslots */
  unsigned long features;  /* special bjl settings */
  unsigned char ESC_r_arg; /* argument used for the ESC (r command during init */
  const char** control_cmdlist;
  const canon_modelist_t* modelist;
  const canon_paperlist_t* paperlist;
  const char *lum_adjustment;
  const char *hue_adjustment;
  const char *sat_adjustment;
} canon_cap_t;

static const char standard_sat_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* B */
/* B */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* M */
/* M */  "1.00 0.95 0.90 0.90 0.90 0.90 0.90 0.90 "  /* R */
/* R */  "0.90 0.95 0.95 1.00 1.00 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* G */
/* G */  "1.00 1.00 1.00 1.00 1.00 1.00 1.00 1.00 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char standard_lum_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"0\" upper-bound=\"4\">\n"
/* C */  "0.65 0.67 0.70 0.72 0.77 0.80 0.82 0.85 "  /* B */
/* B */  "0.87 0.86 0.82 0.79 0.79 0.82 0.85 0.88 "  /* M */
/* M */  "0.92 0.95 0.96 0.97 0.97 0.97 0.96 0.96 "  /* R */
/* R */  "0.96 0.97 0.97 0.98 0.99 1.00 1.00 1.00 "  /* Y */
/* Y */  "1.00 0.97 0.95 0.94 0.93 0.92 0.90 0.86 "  /* G */
/* G */  "0.79 0.76 0.71 0.68 0.68 0.68 0.68 0.66 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char standard_hue_adjustment[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<gutenprint>\n"
"<curve wrap=\"wrap\" type=\"linear\" gamma=\"0\">\n"
"<sequence count=\"48\" lower-bound=\"-6\" upper-bound=\"6\">\n"
/* C */  "0.00 0.06 0.10 0.10 0.06 -.01 -.09 -.17 "  /* B */
/* B */  "-.25 -.33 -.38 -.38 -.36 -.34 -.34 -.34 "  /* M */
/* M */  "-.34 -.34 -.36 -.40 -.50 -.40 -.30 -.20 "  /* R */
/* R */  "-.12 -.07 -.04 -.02 0.00 0.00 0.00 0.00 "  /* Y */
/* Y */  "0.00 0.00 0.00 -.05 -.10 -.15 -.22 -.24 "  /* G */
/* G */  "-.26 -.30 -.33 -.28 -.25 -.20 -.13 -.06 "  /* C */
"</sequence>\n"
"</curve>\n"
"</gutenprint>\n";

static const char* control_cmd_ackshort[] = {
  "AckTime=Short",
  NULL
};

static const char* control_cmd_PIXMA_iP4000[] = {
/*"SetTime=20060722092503", */         /*what is this for?*/
  "SetSilent=OFF",
  "PEdgeDetection=ON",
  "SelectParallel=ECP",
  NULL
};


static const char* control_cmd_MULTIPASS_MP150[] = {
  "AckTime=Short",
  "MediaDetection=ON",
  NULL
};

static const canon_cap_t canon_model_capabilities[] =
{
  /* ******************************** */
  /*                                  */
  /* tested and color-adjusted models */
  /*                                  */
  /* ******************************** */




  /* ************************************ */
  /*                                      */
  /* tested models w/out color-adjustment */
  /*                                      */
  /* ************************************ */

  { /* Canon S200x *//* heads: BC-24 */
    4202, 3,
    618, 936,       /* 8.58" x 13 " */
    10, 10, 9, 20,
    &canon_default_slotlist,
    CANON_CAP_STD1 | CANON_CAP_rr,0x61,NULL,
    &canon_S200_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC S300 */
    8201, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1 | CANON_CAP_r,0x61,control_cmd_ackshort,
    &canon_BJC_8500_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon  BJ 30   *//* heads: BC-10 */
    30, 1,
    9.5*72, 14*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_30_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon  BJC 85  *//* heads: BC-20 BC-21 BC-22 */
    85, 1,
    9.5*72, 14*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_85_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC 4300 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    4300, 1,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_4300_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC 4400 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    4400, 1,
    9.5*72, 14*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_4400_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC 6000 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6000, 3,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1,0,control_cmd_ackshort,
    &canon_BJC_6000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC 6200 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6200, 3,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1,0,control_cmd_ackshort,
    &canon_BJC_6000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  { /* Canon BJC 6500 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6500, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1,0,NULL,
    &canon_BJC_6000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 8200 *//* heads: BC-50 */
    8200, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1 | CANON_CAP_r,0x61,control_cmd_ackshort,
    &canon_BJC_8200_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },


  /* *************** */
  /*                 */
  /* untested models */
  /*                 */
  /* *************** */

  { /* Canon BJC 210 *//* heads: BC-02 BC-05 BC-06 */
    210, 1,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_210_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 240 *//* heads: BC-02 BC-05 BC-06 */
    240, 1,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_240_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 250 *//* heads: BC-02 BC-05 BC-06 */
    250, 1,
    618, 936,      /* 8.58" x 13 " */
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_240_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 1000 *//* heads: BC-02 BC-05 BC-06 */
    1000, 1,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_240_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 2000 *//* heads: BC-20 BC-21 BC-22 BC-29 */
    2000, 1,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_2000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 3000 *//* heads: BC-30 BC-33 BC-34 */
    3000, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL, /*FIX? should have _r? */
    &canon_BJC_3000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 6100 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6100, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1 | CANON_CAP_a | CANON_CAP_r,0x61,NULL,
    &canon_BJC_3000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 7000 *//* heads: BC-60/BC-61 BC-60/BC-62   ??????? */
    7000, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1,0,NULL,
    &canon_BJC_7000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 7100 *//* heads: BC-60/BC-61 BC-60/BC-62   ??????? */
    7100, 3,
    842, 17*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_7100_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },

  /*****************************/
  /*                           */
  /*  extremely fuzzy models   */
  /* (might never work at all) */
  /*                           */
  /*****************************/

  { /* Canon BJC 5100 *//* heads: BC-20 BC-21 BC-22 BC-23 BC-29 */
    5100, 1,
    17*72, 22*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_3000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 5500 *//* heads: BC-20 BC-21 BC-29 */
    5500, 1,
    22*72, 34*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0 | CANON_CAP_a,0,NULL,
    &canon_BJC_5500_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 6500 *//* heads: BC-30/BC-31 BC-32/BC-31 */
    6500, 3,
    17*72, 22*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD1 | CANON_CAP_a,0,NULL,
    &canon_BJC_3000_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon BJC 8500 *//* heads: BC-80/BC-81 BC-82/BC-81 */
    8500, 3,
    17*72, 22*72,
    11, 9, 10, 18,
    &canon_default_slotlist,
    CANON_CAP_STD0,0,NULL,
    &canon_BJC_8500_modelist,
    &canon_default_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon PIXMA iP2000 */
    9001, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px,0x61,control_cmd_PIXMA_iP4000,  /*features */
    &canon_PIXMA_iP2000_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon PIXMA iP3000 */
    3999, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px,0x61,control_cmd_PIXMA_iP4000,  /*features */
    &canon_PIXMA_iP3000_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon PIXMA iP4000 */
    4000, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px /*,|CANON_CAP_I*/,0x64,control_cmd_PIXMA_iP4000,  /*features */
    &canon_PIXMA_iP4000_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon PIXMA iP4200 */
    4200, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px,0x64,control_cmd_PIXMA_iP4000,  /*features */
    &canon_PIXMA_iP4200_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon PIXMA iP6700 */
    4203, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px,0x64,control_cmd_PIXMA_iP4000,  /*features */
    &canon_PIXMA_iP6700_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon MULTIPASS MP150 */
    4201, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_r|CANON_CAP_px|CANON_CAP_I,0x61,control_cmd_MULTIPASS_MP150,  /*features */
    &canon_MULTIPASS_MP150_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
  { /* Canon MULTIPASS MP830 */
    9000, 3,          /*model, model_id*/
    842, 17*72,       /* max paper width and height */
    10, 10, 15, 15,    /*border_left, border_right, border_top, border_bottom */
    &canon_PIXMA_iP4000_slotlist,
    CANON_CAP_STD0|CANON_CAP_DUPLEX|CANON_CAP_px,0,control_cmd_MULTIPASS_MP150,  /*features */
    &canon_MULTIPASS_MP830_modelist,
    &canon_PIXMA_iP4000_paperlist,
    NULL,
    NULL,
    NULL
  },
};

#endif

