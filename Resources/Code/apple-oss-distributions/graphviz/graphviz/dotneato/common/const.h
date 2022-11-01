/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#ifndef PI
#ifdef M_PI
#define PI M_PI
#else
#define PI 3.14159265358979323846
#endif
#endif

#define SMALLBUF	128
#define LPAREN		'('
#define RPAREN		')'
#define LBRACE		'{'
#define RBRACE		'}'

/*	node,edge types */
#ifdef NORMAL
#undef NORMAL
#endif
#define		NORMAL		0		/* an original input node */
#define		VIRTUAL		1		/* virtual nodes in long edge chains */
#define		SLACKNODE	2		/* encode edges in node position phase */
#define		REVERSED	3		/* reverse of an original edge */
#define		FLATORDER	4		/* for ordered edges */
#define		CLUSTER_EDGE 5		/* for ranking clusters */
#define		IGNORED		6		/* concentrated multi-edges */

/* collapsed node classifications */
#define		NOCMD		0		/* default */
#define		SAMERANK	1		/* place on same rank */
#define		MINRANK		2		/* place on "least" rank */
#define		SOURCERANK	3		/* strict version of MINRANK */
#define		MAXRANK		4		/* place on "greatest" rank */
#define		SINKRANK	5		/* strict version of MAXRANK */
#define		LEAFSET		6		/* set of collapsed leaf nodes */
#define		CLUSTER		7		/* set of clustered nodes */

/* type of cluster rank assignment */
#define		LOCAL		100
#define		GLOBAL		101
#define		NOCLUST		102

/* default attributes */
#define		DEFAULT_COLOR		"black"
#define		DEFAULT_FONTSIZE	14.0
#define		DEFAULT_LABEL_FONTSIZE	11.0 /* for head/taillabel */
#define		MIN_FONTSIZE		1.0
#define		DEFAULT_FONTNAME	"Times-Roman"
#define		DEFAULT_FILL		"lightgrey"
#define		LINESPACING		1.20

#define		DEFAULT_NODEHEIGHT	0.5
#define		MIN_NODEHEIGHT		0.02
#define		DEFAULT_NODEWIDTH	0.75
#define		MIN_NODEWIDTH		0.01
#define		DEFAULT_NODESHAPE	"ellipse"

#define		NODENAME_ESC		"\\N"

#define		DEFAULT_LAYERSEP	":\t "

#define		DEFAULT_NODESEP	0.25
#define		MIN_NODESEP		0.02	
#define		DEFAULT_RANKSEP	0.5
#define		MIN_RANKSEP		0.02

/* default margin for paged formats such as PostScript */
#define		DEFAULT_MARGIN	36

/* default margin for embedded formats such as PNG */
#define		DEFAULT_EMBED_MARGIN	5

#define		DEFAULT_PAGEHT	792
#define		DEFAULT_PAGEWD	612

#define		SELF_EDGE_SIZE	18
#define		MC_SCALE	256	/* for mincross */

#define		PORT_LABEL_DISTANCE	10
#define		PORT_LABEL_ANGLE	-25 /* degrees; pos is CCW, neg is CW */

/* arrow types */
#define		ARR_NONE         0

/* sides (e.g. of cluster margins) */
#define		BOTTOM_IX	0
#define		RIGHT_IX	1
#define		TOP_IX		2
#define		LEFT_IX		3

/* sides of boxes for SHAPE_path */
#define		BOTTOM		(1<<BOTTOM_IX)
#define		RIGHT		(1<<RIGHT_IX)
#define		TOP		(1<<TOP_IX)
#define		LEFT		(1<<LEFT_IX)

/* Obj types  for renderers */
#define		NONE    0
#define		NODE    1
#define		EDGE    2
#define		CLST    3

/* output languages */
#define		ATTRIBUTED_DOT	0	/* default */
#define		POSTSCRIPT	1
#define		HPGL		2	/* HP Graphics Language */
#define		PCL		3	/* Printer Control Language */
#define		MIF		4	/* Adobe FrameMaker */
#define		PIC_format	5	/* symbol PIC is used by compilers for 
					   Position Independent Code */
#define		PLAIN		6
#define		PLAIN_EXT	7

#define		GD		8	/* libgd bitmap format */
#define		memGD		9	/* libgd bitmap format */
#define		GD2		10	/* libgd bitmap format */
#define		GIF		11	/* libgd bitmap format */
#define		JPEG		12	/* libgd bitmap format */
#define		PNG		13	/* libgd bitmap format */
#define		WBMP		14	/* libgd bitmap format */
#define		XBM		15	/* libgd bitmap format */

#define 	ISMAP		16	/* old style map file for httpd servers */
#define 	IMAP		17	/* apache map file for httpd servers */
#define 	CMAP		18	/* client map file for html browsers */
#define 	CMAPX		19	/* client map file for xhtml browsers */
#define		VRML		20
#define		VTX		21	/* visual thought */
#define		METAPOST	22
#define		FIG		23	/* xfig drawing tool */
#define		DIA		24	/* dia drawing tool */
#define		SVG		25	/* Structured Vector Graphics */
#define		SVGZ		26	/* compressed SVG */
#define		CANONICAL_DOT	27	/* wanted for tcl/tk version */
#define		PDF		28
#define		EXTENDED_DOT	29	/* dot with drawing info */

/* for clusters */
#define		CL_BACK		10		/* cost of backward pointing edge */
#define		CL_OFFSET	8		/* margin of cluster box in PS points */
#ifndef MSWIN32
#define		CL_CROSS	1000	/* cost of cluster skeleton edge crossing */
#else
#define		CL_CROSS	100		/* avoid 16 bit overflow */
#endif

/* for graph server */
#define		SERVER_NN	200
#define		SERVER_NE	500

/* for neato */
#define Spring_coeff    1.0
#define MYHUGE          (1.0e+37)
#define MAXDIM			10

/* drawing phases */
#define GVBEGIN         0
#define GVSPLINES       1

/* existence of labels */
#define EDGE_LABEL		1
#define HEAD_LABEL		2
#define TAIL_LABEL		4
#define GRAPH_LABEL		8

/* type of graph label: GD_label_pos */
#define LABEL_AT_BOTTOM	0
#define LABEL_AT_TOP	1
#define LABEL_AT_LEFT	2
#define LABEL_AT_RIGHT	4

/* user-specified node position: ND_pinned */
#define P_SET    1       /* position supplied by user */
#define P_FIX    2       /* position fixed during topological layout */
#define P_PIN    3       /* position fixed */

#define GAP (PS2INCH(4.))   /* whitespace around labels and between peripheries */

