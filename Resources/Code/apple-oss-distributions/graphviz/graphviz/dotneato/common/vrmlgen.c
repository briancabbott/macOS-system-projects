/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped

#include		"render.h"
#include		"gd.h"
#include		"utils.h"


extern char *get_ttf_fontpath(char *fontreq, int warn);

#ifdef HAVE_LIBPNG

#ifndef MAXFLOAT
#define MAXFLOAT 10000000.
#endif

#define		NONE		0
#define		NODE		1
#define		EDGE		2
#define		CLST		3

#define SCALE ((double)GD_RESOLUTION/(double)POINTS_PER_INCH)

#define BEZIERSUBDIVISION 10

/* font modifiers */
#define REGULAR 0
#define BOLD		1
#define ITALIC		2

/* patterns */
#define P_SOLID		0
#define P_NONE  15
#define P_DOTTED 4				/* i wasn't sure about this */
#define P_DASHED 11				/* or this */

/* bold line constant */
#define WIDTH_NORMAL 1
#define WIDTH_BOLD 3

typedef struct {
	unsigned char r, g, b;
} Color;


/* static int	N_pages; */
/* static point	Pages; */
static double	Scale;
static int	Rot;
/* static box	PB; */
/* static int	onetime = TRUE; */
static int	Saw_skycolor;

static gdImagePtr im;
static FILE		*PNGfile;
static node_t	*Curnode;
static edge_t	*Curedge;

typedef struct context_t {
	unsigned char   pencolor_ix, fillcolor_ix;
	char		*pencolor, *fillcolor;
	char		*fontfam, fontopt, font_was_set;
	double		r,g,b;
	char		pen, fill, penwidth;
	double		fontsz;
} context_t;

#define MAXNEST 4
static context_t cstk[MAXNEST];
static int	  SP;

static double dist2(pointf p, point q)
{
	return ((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y));
}


static char *nodeURL(node_t *n, char *buf)
{
	sprintf(buf,"node%d.png",n->id);
	return buf;
}

/* gdirname:
 * Returns directory pathname prefix
 * Code adapted from dgk
 */
static char*
gdirname (char* pathname)
{
    char* last;

    /* go to end of path */
    for(last=pathname; *last; last++);
    /* back over trailing '/' */
    while(last>pathname && *--last=='/');
    /* back over non-slash chars */
    for(;last>pathname && *last!='/';last--);
    if(last==pathname)
    {
        /* all '/' or "" */
        if(*pathname!='/')
            *last = '.';
        /* preserve // */
        else if(pathname[1]=='/')
            last++;
    }
    else
    {
        /* back over trailing '/' */
        for(;*last=='/' && last > pathname; last--);
        /* preserve // */
        if(last==pathname && *pathname=='/' && pathname[1]=='/')
            last++;
    }
    last++;
    *last = '\0';

    return pathname;
}

static char *nodefilename(node_t *n, char *buf)
{
	static char *dir;
	static char disposable[1024];
	char junkbuf[1024];

	if (dir == 0) {
		if (Output_file_name)
			dir = gdirname(strcpy(disposable,Output_file_name));
		else dir = ".";
	}
	sprintf(buf,"%s/%s", dir, nodeURL(n,junkbuf));
	return buf;
}

static FILE *nodefile(node_t *n)
{
	FILE	*rv;
	char	buf[1024];

	rv = fopen(nodefilename(n,buf),"wb");
	return rv;
}

static unsigned char vrml_resolve_color(char* name)
{
        color_t color;

        if (!(strcmp(name,"transparent"))) {
                /* special case for "transparent" color */
                return gdImageColorResolve(im,255,255,254);
        } 
        else {
		colorxlate(name,&color,RGBA_BYTE);
		return gdImageColorResolve(im,
			color.u.rgba[0],color.u.rgba[1],color.u.rgba[2]);
	}
}

static void vrml_set_pencolor(char* name)
{
	cstk[SP].pencolor = name;
}

static void vrml_set_fillcolor(char* name)
{
	cstk[SP].fillcolor = name;
}

static void init_png(gdImagePtr im)
{
	int transparent;

	if ((transparent = gdImageGetTransparent(im)) == -1) {
		transparent = gdImageColorResolve(im, 255, 255, 254);
		gdImageColorTransparent(im, transparent);
	}
}

static pointf vrml_node_point(point p)
{
	pointf            rv;

        /* make mp relative to PNG canvas */
	if (Rot == 0) {
	        rv.x = (p.x - ND_coord_i(Curnode).x + ND_lw_i(Curnode)) * Scale;
	        rv.y = (ND_coord_i(Curnode).y - p.y + ND_ht_i(Curnode)/2) * Scale;
	} else {
	        rv.x = (p.y - ND_coord_i(Curnode).y + ND_lw_i(Curnode)) * Scale;
	        rv.y = (ND_coord_i(Curnode).x - p.x + ND_ht_i(Curnode)/2) * Scale;
	}
	return rv;
}

static void vrml_font(context_t* cp)
{
/* FIX
	char		   *fw, *fa;

	fw = fa = "Regular";
	switch (cp->fontopt) {
	case BOLD:
		fw = "Bold";
		break;
	case ITALIC:
		fa = "Italic";
		break;
	}
*/
}

/* warmed over VRML code starts here */

static void vrml_begin_job(FILE *ofp, graph_t *g, char **lib, char *user,
char *info[], point pages)
{
	fprintf(Output_file, "#VRML V2.0 utf8\n");
}

static void vrml_begin_graph(graph_t* g, box bb, point pb)
{
	g = g;
#if 0 /* not used */
	PB.LL.x = bb.LL.x * SCALE;
	PB.LL.y = bb.LL.y * SCALE;
	PB.UR.x = bb.UR.x * SCALE;
	PB.UR.y = bb.UR.y * SCALE;
#endif

	Saw_skycolor = FALSE;
	fprintf(Output_file, "Group { children [\n");
	fprintf(Output_file,"  Viewpoint {position %.3f %.3f 10}\n",
		.0278*(bb.UR.x+bb.LL.x)/2.0, .0278*(bb.UR.y+bb.LL.y)/2.0);
	fprintf(Output_file,"  Transform {\n");
	fprintf(Output_file,"    scale %.3f %.3f %.3f\n",
		.0278 , .0278 , .0278 );
	fprintf(Output_file,"    children [\n");

	SP = 0;
	cstk[0].fillcolor = "white";
	cstk[0].fontfam = "times";	/* font family name */
	cstk[0].fontopt = REGULAR;	/* modifier: REGULAR, BOLD or ITALIC */
	cstk[0].pen = P_SOLID;		/* pen pattern style, default is solid */
	cstk[0].fill = P_NONE;
	cstk[0].penwidth = WIDTH_NORMAL;
}

static void vrml_end_graph(void)
{
	if (!Saw_skycolor) 
		fprintf(Output_file," Background { skyColor 1 1 1 }\n");
	fprintf(Output_file,"  ] }\n");
	fprintf(Output_file, "] }\n");
}

static void vrml_begin_page(graph_t *g, point page, double scale, int rot, point offset)
{
#if 0 /* scale not used */
	Scale = scale * SCALE;
#else
	Scale = SCALE;
#endif
	Rot = rot;
}

static void
vrml_begin_node(node_t* n)
{
	int		width, height;

	PNGfile = nodefile(n);
	width = (ND_lw_i(n) + ND_rw_i(n))*Scale + 3;
	height = (ND_ht_i(n))*Scale + 3;
	im = gdImageCreate(width, height);
	init_png(im);
	Curnode = n;
}

static void
vrml_end_node(void)
{
	gdImagePng(im, PNGfile);
	gdImageDestroy(im);
	im = 0;
	fclose(PNGfile);
}

static  void
vrml_begin_edge(edge_t* e)
{
	Curedge = e;
	fprintf(Output_file," Group { children [\n");
}

static  void
vrml_end_edge(void)
{
	fprintf(Output_file,"] }\n");
}

static  void
vrml_begin_context(void)
{
	assert(SP + 1 < MAXNEST);
	cstk[SP + 1] = cstk[SP];
	SP++;
}

static  void
vrml_end_context(void)
{
	int			 psp = SP - 1;
	assert(SP > 0);
	if (cstk[SP].font_was_set)
		vrml_font(&(cstk[psp]));
	/* free(cstk[psp].fontfam); */
	SP = psp;
}

static  void
vrml_set_font(char* name, double size)
{
	char		   *p, *q;
	context_t	  *cp;

	cp = &(cstk[SP]);
	cp->font_was_set = TRUE;
	cp->fontsz = size;
	p = strdup(name);
	if ((q = strchr(p, '-'))) {
		*q++ = 0;
		if (strcasecmp(q, "italic") == 0)
			cp->fontopt = ITALIC;
		else if (strcasecmp(q, "bold") == 0)
			cp->fontopt = BOLD;
	}
	cp->fontfam = p;
	vrml_font(&cstk[SP]);
}

static  void
vrml_set_style(char** s)
{
	char		   *line;
	context_t	  *cp;

	cp = &(cstk[SP]);
	while ((line = *s++)) {
		if (streq(line, "solid")) cp->pen = P_SOLID;
		else if (streq(line, "dashed")) cp->pen = P_DASHED;
 		else if (streq(line, "dotted")) cp->pen = P_DOTTED;
 		else if (streq(line, "bold")) cp->penwidth = WIDTH_BOLD;
		else if (streq(line, "invis")) cp->pen = P_NONE;
		else if (streq(line, "filled")) cp->fill = P_SOLID;
		else if (streq(line, "unfilled")) cp->fill = P_NONE;
		else {
			agerr(AGERR, "vrml_set_style: unsupported style %s - ignoring\n",
					line);
		}
	}
}

static void
vrml_textline(point p, textline_t *line)
{
	char		*fontlist, *err;
	pointf		mp;
	int		brect[8];
	extern          gdFontPtr       gdFontSmall;

	if (Obj != NODE) return;
	cstk[SP].pencolor_ix = vrml_resolve_color(cstk[SP].pencolor);
	fontlist = gd_alternate_fontlist(cstk[SP].fontfam);

        switch(line->just) {
		case 'l':
			break;
		case 'r':
			p.x -= line->width;
			break;
		default:
		case 'n':
			p.x -= line->width / 2;
			break;
	}
/*	p.y += cstk[SP].fontsz*2/3; */

	mp = vrml_node_point(p);

	err = gdImageStringFT(im, brect, cstk[SP].pencolor_ix, fontlist, 
		cstk[SP].fontsz, (Rot? 90.0 : 0.0) * PI / 180.0,
		ROUND(mp.x), ROUND(mp.y), line->str);
	if (err) {
		/* revert to builtin fonts */
		gdImageString(im, gdFontSmall, ROUND(mp.x), ROUND(mp.y),
                        (unsigned char *)line->str, cstk[SP].pencolor_ix);
	}
}

static double interpolate_zcoord(pointf p1, point fst, double fstz, point snd, double sndz)
{
	double	rv;

	if (fstz == sndz) return fstz;
#define FIX 1  /* i wonder why wasn't this enabled? scn 9/15/2002 */
#ifdef FIX
	if (ND_rank(Curedge->tail) != ND_rank(Curedge->head)) {
		if (snd.y == fst.y) rv = (fstz + sndz) / 2.0;
		else rv = fstz + (sndz - fstz) * (p1.y - fst.y)/(snd.y - fst.y);
	}
	else
#endif
		if (snd.x == fst.x) rv = (fstz + sndz) / 2.0;
		else rv = fstz + (sndz - fstz) * (p1.x - fst.x)/(snd.x - fst.x);
	return rv;
}

static  void
vrml_bezier(point* A, int n, int arrow_at_start, int arrow_at_end)
{
	pointf		p1, V[4];
	int			i, j, step;
	double		fstz, sndz;
	context_t	  *cp;

	assert(Obj == EDGE);

	cp = &(cstk[SP]);
	if (cp->pen == P_NONE) return;
	fstz = late_double(Curedge->tail, N_z, 0.0, -1000.0);
	sndz = late_double(Curedge->head, N_z, 0.0, -MAXFLOAT);
	fprintf(Output_file,"Shape { geometry Extrusion  {\n");
	fprintf(Output_file,"  spine [");
	V[3].x = A[0].x; V[3].y = A[0].y;
	for (i = 0; i+3 < n; i += 3) {
		V[0] = V[3];
		for (j = 1; j <= 3; j++) {
			V[j].x  = A[i+j].x; V[j].y = A[i+j].y;
		}
		for (step = 0; step <= BEZIERSUBDIVISION; step++) {
			p1 = Bezier(V, 3, (double)step/BEZIERSUBDIVISION, NULL, NULL);
			fprintf(Output_file," %.3f %.3f %.3f", p1.x, p1.y,
				interpolate_zcoord(p1,A[0],fstz,A[n-1],sndz));
		}
	}
	fprintf(Output_file," ]\n");
	fprintf(Output_file, "  crossSection [ %d %d, %d %d, %d %d, %d %d ]\n",
		(cp->penwidth), (cp->penwidth), -(cp->penwidth), (cp->penwidth),
		-(cp->penwidth), -(cp->penwidth), (cp->penwidth), -(cp->penwidth));
	fprintf(Output_file,"}\n");
	fprintf(Output_file," appearance DEF E%d Appearance {\n",Curedge->id);
	fprintf(Output_file,"   material Material {\n");
	fprintf(Output_file,"   ambientIntensity 0.33\n");
	fprintf(Output_file,"   diffuseColor %.3f %.3f %.3f\n",
		cstk[SP].r, cstk[SP].g, cstk[SP].b);
	fprintf(Output_file,"   }\n");
	fprintf(Output_file," }\n");
	fprintf(Output_file,"}\n");
}

static  void
vrml_polygon(point *A, int n, int filled)
{
	pointf		p, mp;
	int		i;
	gdPoint         *points;
	int             style[20];
	int             pen, width;
	gdImagePtr      brush = NULL;
	double		theta, z;
	node_t		*endp;
	char		somebuf[1024];

	switch (Obj) {
	case NONE:	 /* GRAPH */
		fprintf(Output_file," Background { skyColor %.3f %.3f %.3f }\n",
			cstk[SP].r, cstk[SP].g, cstk[SP].b);
		Saw_skycolor = TRUE;
		break;

	case NODE:

	if (cstk[SP].pen != P_NONE) {
		cstk[SP].pencolor_ix = vrml_resolve_color(cstk[SP].pencolor);
		cstk[SP].fillcolor_ix = vrml_resolve_color(cstk[SP].fillcolor);
                if (cstk[SP].pen == P_DASHED) {
                        for (i = 0; i < 10; i++)
                                style[i] = cstk[SP].pencolor_ix;
                        for (; i < 20; i++)
                                style[i] = gdTransparent;
                        gdImageSetStyle(im, style, 20);
                        pen = gdStyled;
                } else if (cstk[SP].pen == P_DOTTED) {
                        for (i = 0; i < 2; i++)
                                style[i] = cstk[SP].pencolor_ix;
                        for (; i < 12; i++)
                                style[i] = gdTransparent;
                        gdImageSetStyle(im, style, 12);
                        pen = gdStyled;
                } else {
                        pen = cstk[SP].pencolor_ix;
                }
                if (cstk[SP].penwidth != WIDTH_NORMAL) {
                        width=cstk[SP].penwidth;
                        brush = gdImageCreate(width,width);
                        gdImagePaletteCopy(brush, im);
                        gdImageFilledRectangle(brush,
                           0,0,width-1, width-1, cstk[SP].pencolor_ix);
                        gdImageSetBrush(im, brush);
                        if (pen == gdStyled) pen = gdStyledBrushed;
                        else pen = gdBrushed;
                }
                points = N_GNEW(n,gdPoint);
                for (i = 0; i < n; i++) {
			mp = vrml_node_point(A[i]);
                        points[i].x = ROUND(mp.x); points[i].y = ROUND(mp.y);
                }
                if (filled) gdImageFilledPolygon(im,
				points, n, cstk[SP].fillcolor_ix);
                gdImagePolygon(im, points, n, pen);
                free(points);
                if (brush)
                        gdImageDestroy(brush);
        }

	z = late_double(Curnode,N_z,0.0,-MAXFLOAT);

	fprintf(Output_file,"Shape {\n");
	fprintf(Output_file,"  appearance Appearance {\n");
	fprintf(Output_file,"    material Material {\n");
	fprintf(Output_file,"      ambientIntensity 0.33\n");
	fprintf(Output_file,"        diffuseColor 1 1 1\n");
	fprintf(Output_file,"    }\n");
	fprintf(Output_file,"    texture ImageTexture { url \"%s\" }\n",
		nodeURL(Curnode,somebuf));
	fprintf(Output_file,"  }\n");
	fprintf(Output_file,"  geometry Extrusion {\n");
	fprintf(Output_file,"    crossSection [");
	for (i = 0; i < n; i++) {
		p.x = A[i].x - ND_coord_i(Curnode).x;
		p.y = A[i].y - ND_coord_i(Curnode).y;
		fprintf(Output_file," %.3f %.3f,", p.x, p.y);
	}
	p.x = A[0].x - ND_coord_i(Curnode).x;
	p.y = A[0].y - ND_coord_i(Curnode).y;
	fprintf(Output_file," %.3f %.3f ]\n", p.x, p.y);
	fprintf(Output_file, "    spine [ %d %d %.3f, %d %d %.3f ]\n",
		ND_coord_i(Curnode).x, ND_coord_i(Curnode).y, z - .01,
		ND_coord_i(Curnode).x, ND_coord_i(Curnode).y, z + .01);
	fprintf(Output_file,"  }\n");
	fprintf(Output_file,"}\n");
	break;

	case EDGE:
		if (cstk[SP].pen == P_NONE) return;
		p.x = p.y = 0.0;
		for (i = 0; i < n; i++) {
			p.x += A[i].x;
			p.y += A[i].y;
		}
		p.x = p.x / n; p.y = p.y / n;

		/* it is bad to know that A[1] is the aiming point, but we do */
		theta = atan2((A[0].y + A[2].y)/2.0 - A[1].y, (A[0].x + A[2].x)/2.0 - A[1].x) + PI / 2.0;


		/* this is gruesome, but how else can we get z coord */
		if (dist2(p,ND_coord_i(Curedge->tail)) < dist2(p,ND_coord_i(Curedge->head)))
			endp = Curedge->tail;
		else
			endp = Curedge->head;
		z = late_double(endp,N_z,0.0,-MAXFLOAT);

		/* FIXME: arrow vector ought to follow z coord of bezier */
		fprintf(Output_file,"Transform {\n");
		fprintf(Output_file,"  translation %.3f %.3f %.3f\n", p.x, p.y, z);
		fprintf(Output_file,"  children [\n");
		fprintf(Output_file,"    Transform {\n");
		fprintf(Output_file,"      rotation 0 0 1 %.3f\n", theta);
		fprintf(Output_file,"      children [\n");
		fprintf(Output_file,"        Shape {\n");
		fprintf(Output_file,"          geometry Cone {bottomRadius %.3f height %.3f }\n",cstk[SP].penwidth * 2.5,cstk[SP].penwidth * 10.0);
		fprintf(Output_file,"          appearance USE E%d\n",Curedge->id);
		fprintf(Output_file,"        }\n");
		fprintf(Output_file,"      ]\n");
		fprintf(Output_file,"    }\n");
		fprintf(Output_file,"  ]\n");
		fprintf(Output_file,"}\n");
		break;
	default:
		break;
	}
}

static  void
vrml_ellipse(point p, int rx, int ry, int filled)
{
	pointf		mp;
	int		i;
	node_t		*endp;
	int             style[40];  /* need 2* size for arcs, I don't know why */
	int             pen, width;
	gdImagePtr      brush = NULL;
	double		z;
	char		somebuf[1024];

	switch (Obj) {
	case NODE:
		cstk[SP].pencolor_ix = vrml_resolve_color(cstk[SP].pencolor);
		cstk[SP].fillcolor_ix = vrml_resolve_color(cstk[SP].fillcolor);
	        if (cstk[SP].pen != P_NONE) {
	                if (cstk[SP].pen == P_DASHED) {
	                        for (i = 0; i < 20; i++)
	                                style[i] = cstk[SP].pencolor_ix;
	                        for (; i < 40; i++)
	                                style[i] = gdTransparent;
	                        gdImageSetStyle(im, style, 40);
	                        pen = gdStyled;
	                } else if (cstk[SP].pen == P_DOTTED) {
	                        for (i = 0; i < 2; i++)
	                                style[i] = cstk[SP].pencolor_ix;
	                        for (; i < 24; i++)
	                                style[i] = gdTransparent;
	                        gdImageSetStyle(im, style, 24);
	                        pen = gdStyled;
	                } else {
	                        pen = cstk[SP].pencolor_ix;
	                }
	                if (cstk[SP].penwidth != WIDTH_NORMAL) {
	                        width = cstk[SP].penwidth;
	                        brush = gdImageCreate(width,width);
	                        gdImagePaletteCopy(brush, im);
	                        gdImageFilledRectangle(brush,
	                           0,0,width-1, width-1, cstk[SP].pencolor_ix);
	                        gdImageSetBrush(im, brush);
	                        if (pen == gdStyled) pen = gdStyledBrushed;
	                        else pen = gdBrushed;
	                }
			mp = vrml_node_point(p);
	
	                if (filled) {
	                        gdImageFilledEllipse(im, ROUND(mp.x), ROUND(mp.y),
	                                ROUND(Scale*(rx + rx)), ROUND(Scale*(ry + ry)),
	                                cstk[SP].fillcolor_ix);
	                }
	                gdImageArc(im, ROUND(mp.x), ROUND(mp.y),
	                        ROUND(Scale*(rx + rx)), ROUND(Scale*(ry + ry)), 0, 360, pen);
	                if (brush)
	                        gdImageDestroy(brush);
	        }
	
		mp.x = ND_coord_i(Curnode).x;
		mp.y = ND_coord_i(Curnode).y;
	
		z = late_double(Curnode,N_z,0.0,-MAXFLOAT);
		fprintf(Output_file,"Transform {\n");
		fprintf(Output_file,"  translation %.3f %.3f %.3f\n", mp.x, mp.y, z);
		fprintf(Output_file,"  scale %d %d 1\n", rx, ry);
		fprintf(Output_file,"  children [\n");
		fprintf(Output_file,"    Transform {\n");
		fprintf(Output_file,"      rotation 1 0 0   1.57\n");
		fprintf(Output_file,"      children [\n");
		fprintf(Output_file,"        Shape {\n");
		fprintf(Output_file,"          geometry Cylinder { side FALSE }\n");
		fprintf(Output_file,"          appearance Appearance {\n");
		fprintf(Output_file,"            material Material {\n");
		fprintf(Output_file,"              ambientIntensity 0.33\n");
		fprintf(Output_file,"              diffuseColor 1 1 1\n");
		fprintf(Output_file,"            }\n");
		fprintf(Output_file,"            texture ImageTexture { url \"%s\" }\n",
			nodeURL(Curnode,somebuf));
		fprintf(Output_file,"          }\n");
		fprintf(Output_file,"        }\n");
		fprintf(Output_file,"      ]\n");
		fprintf(Output_file,"    }\n");
		fprintf(Output_file,"  ]\n");
		fprintf(Output_file,"}\n");
		break;
	case EDGE:
		if (cstk[SP].pen == P_NONE) return;
		mp.x = (double)p.x;
		mp.y = (double)p.y;
		/* this is gruesome, but how else can we get z coord */
		if (dist2(mp,ND_coord_i(Curedge->tail)) < dist2(mp,ND_coord_i(Curedge->head)))
			endp = Curedge->tail;
		else
			endp = Curedge->head;
		z = late_double(endp,N_z,0.0,-MAXFLOAT);
		fprintf(Output_file,"Transform {\n");
		fprintf(Output_file,"  translation %.3f %.3f %.3f\n", mp.x, mp.y, z);
		fprintf(Output_file,"  children [\n");
		fprintf(Output_file,"    Shape {\n");
		fprintf(Output_file,"      geometry Sphere {radius %.3f }\n",(double)rx);
		fprintf(Output_file,"      appearance USE E%d\n",Curedge->id);
		fprintf(Output_file,"    }\n");
		fprintf(Output_file,"  ]\n");
		fprintf(Output_file,"}\n");
		break;
	default:
		break;
	}
}

static  void
vrml_polyline(point* A, int n)
{
/*
	pointf		  p, p1;
	int			 i;

	if (cstk[SP].pen != P_NONE) {
		p.x = A[0].x;
		p.y = A[0].y;
		for (i = 1; i < n; i++) {
			p1.x = A[i].x;
			p1.y = A[i].y;
#ifdef NONEOFTHISEITHER
			if (cstk[SP].pen == P_DASHED) {
				gdImageDashedLine(im, ROUND(p.x), ROUND(p.y),
					ROUND(p1.x), ROUND(p1.y), cstk[SP].color_ix);
			} else {
				gdImageLine(im, ROUND(p.x), ROUND(p.y),
					ROUND(p1.x), ROUND(p1.y), cstk[SP].color_ix);
			}
#endif
			p.x = p1.x;
			p.y = p1.y;
		}
	}
*/
}

static  void
vrml_user_shape(char *name, point *A, int n, int filled)
{
	vrml_polygon(A, n, filled);
}

codegen_t	   VRML_CodeGen = {
	0, /* vrml_reset */
	vrml_begin_job, 0, /* vrml_end_job */
	vrml_begin_graph, vrml_end_graph,
	vrml_begin_page, 0, /* vrml_end_page */
	0, /* vrml_begin_layer */ 0, /* vrml_end_layer */
	0, /* vrml_begin_cluster */ 0, /* vrml_end_cluster */
	0, /* vrml_begin_nodes */ 0, /* vrml_end_nodes */
	0, /* vrml_begin_edges */ 0, /* vrml_end_edges */
	vrml_begin_node, vrml_end_node,
	vrml_begin_edge, vrml_end_edge,
	vrml_begin_context, vrml_end_context,
	vrml_set_font, vrml_textline,
	vrml_set_pencolor, vrml_set_fillcolor, vrml_set_style,
	vrml_ellipse, vrml_polygon,
	vrml_bezier, vrml_polyline,
	0, /* bezier_has_arrows */
	0, /* comment */
	gd_textsize,
	vrml_user_shape,
	0 /* vrml_usershapesize */
};
#endif /* HAVE_LIBPNG */
