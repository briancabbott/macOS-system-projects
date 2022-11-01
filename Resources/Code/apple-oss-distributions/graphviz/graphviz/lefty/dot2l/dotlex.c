#pragma prototyped
/* the graph lexer */

typedef void *Tobj;
#include "common.h"
#include "dotparse.h"
#include "dot2l.h"
#include "io.h"
#include "triefa.c"

static int Syntax_errors;
static int Lexer_fd;
#define LEXBUFSIZE 10240
static char LexBuf[LEXBUFSIZE], *LexPtr;
static int In_comment;
static int Comment_start;
int Line_number;

static char *lex_gets (void);
static int lex_token (char *);
static void error_context (void);
static char *skip_wscomments (char *);
static char *scan_token (char *);
static char *scan_num (char *);
static char *quoted_string (char *);

void lex_begin (int ioi) {
    Lexer_fd = ioi;
    LexPtr = NULL;
}

int myyylex (void) {        /* for debugging */
    int rv = myyylex ();

    fprintf (stderr, "returning %d\n", rv);
    if (rv == T_id)
        fprintf (stderr, "string val is %s\n", yylval.s);
    return rv;
}

int yylex (void) {
    int token;
    char *p;

    /* if the parser has accepted a graph, reset and return EOF */
    if (yaccdone) {
        yaccdone = FALSE;
        return EOF;
    }

    /* get a nonempty lex buffer */
    do {
        if ((LexPtr == NULL) || (LexPtr[0] == '\0'))
            if ((LexPtr = lex_gets ()) == NULL) {
                if (In_comment)
                    fprintf (stderr,
                            "warning, nonterminated comment in line %d\n",
                            Comment_start);
                return EOF;
            }
        LexPtr = skip_wscomments (LexPtr);
    } while (LexPtr[0] == '\0');

    /* scan quoted strings */
    if (LexPtr[0] == '\"') {
        LexPtr = quoted_string (LexPtr);
        yylval.s = (char *) strdup (LexBuf);
        return T_id;
    }

    /* scan edge operator */
    if (etype && (strncmp (LexPtr, etype, strlen (etype)) == 0)) {
        LexPtr += strlen (etype);
        return T_edgeop;
    }

    /* scan numbers */
    if ((p = scan_num (LexPtr))) {
        LexPtr = p;
        yylval.s =  strdup (LexBuf);
        return T_id;
    }
    else {
        if (ispunct (LexPtr[0]) && (LexPtr[0] != '_'))
            return *LexPtr++;
        else
            LexPtr = scan_token (LexPtr);
    }

    /* scan other tokens */
    token = lex_token (LexBuf);
    if (token == -1) {
        yylval.s = strdup (LexBuf);
        token = T_id;
    }
    return token;
}

void
yyerror (char *fmt, char *s) {
    if (Syntax_errors++)
        return;
    fprintf (stderr, "graph parser: ");
    fprintf (stderr, fmt, s);
    fprintf (stderr, " near line %d\n", Line_number);
    error_context ();
}

static char *lex_gets (void) {
    char *clp;
    int len, curlen;

    curlen = 0;
    do {
            /* off by one so we can back up in LineBuf */
        if (IOreadline (Lexer_fd,
                LexBuf + curlen + 1, LEXBUFSIZE - curlen - 1) == -1)
            break;
        clp = LexBuf + curlen + 1;
        len = strlen (clp);
        clp[len++] = '\n';
        clp[len] = 0;

        if (clp == LexBuf + 1 && clp[0] == '#') {
            /* comment line or cpp line sync */
            if (sscanf (clp+1, "%d", &Line_number) == 0)
                Line_number++;
            len = 0;
            clp[len] = 0;
            continue;
        }

        Line_number++;
        if ((len = strlen (clp)) > 1) {
            if (clp[len - 2] == '\\') {
                len = len - 2;
                clp[len] = '\0';
            }
        }
        curlen += len;
    } while (clp[len - 1] != '\n');

    if (curlen > 0)
        return LexBuf + 1;
    else
        return NULL;
}

static int lex_token (char *p) {
    TFA_Init ();
    while (*p)
        TFA_Advance (*p++);
    return
        TFA_Definition ();
}

static void error_context (void) {
    char *p, *q;

    if (LexPtr == NULL)
        return;
    fprintf (stderr, "context: ");
    for (p = LexPtr - 1; (p > LexBuf) && (isspace (*p) == FALSE); p--)
        ;
    for (q = LexBuf; q < p; q++)
        fputc (*q, stderr);
    fputs (" >>> ", stderr);
    for (; q < LexPtr; q++)
        fputc (*q, stderr);
    fputs (" <<< ", stderr);
    fputs (LexPtr, stderr);
}

/* i wrote this and it still frightens me */
/* skip white space and comments in p */
static char *skip_wscomments (char *p) {
    do {
        while (isspace (*p))
            p++;
        while (In_comment && p[0]) {
            while (p[0] && (p[0] != '*'))
                p++;
            if (p[0]) {
                if (p[1] == '/') {
                    In_comment = FALSE;
                    p += 2;
                    break;
                } else
                    p++;
            }
        }
        if (p[0] == '/') {
            if (p[1] == '/')
                while (*p)
                    p++;    /* skip to end of line */
            else {
                if (p[1] == '*') {
                    In_comment = TRUE;
                    Comment_start = Line_number;
                    p += 2;
                    continue;
                }
                else
                    break;    /* return a slash */
            }
        } else {
            if (!isspace (*p))
                break;
        }
    } while (p[0]);
    return p;
}

/* scan an unquoted token and return the position after its terminator */
static char *scan_token (char *p) {
    char *q;

    q = LexBuf;
    if (p == '\0')
        return NULL;
    while (isalnum (*p) || (*p == '_'))
        *q++ = *p++;
    *q = '\0';
    return p;
}

static char *scan_num (char *p) {
    char *q, *z;
    int saw_rp = FALSE;
    int saw_digit = FALSE;

    z = p;
    q = LexBuf;
    if (*z == '-')
        *q++ = *z++;
    if (*z == '.') {
        saw_rp = TRUE;
        *q++ = *z++;
    }
    while (isdigit (*z)) {
        saw_digit = TRUE;
        *q++ = *z++;
    }
    if ((*z == '.') && (saw_rp == FALSE)) {
        saw_rp = TRUE;
        *q++ = *z++;
        while (isdigit (*z)) {
            saw_digit = TRUE;
            *q++ = *z++;
        }
    }
    *q = '\0';
    if (saw_digit && *z && (isalpha (*z)))
        yyerror ("badly formed number %s", LexBuf);

    if (saw_digit == FALSE)
        z = NULL;
    return z;
}

/* scan a quoted string and return the position after its terminator */
static char *quoted_string (char *p) {
    char quote, *q;

    quote = *p++;
    q = LexBuf;
    while ((*p) && (*p != quote)) {
        if (*p == '\\') {
            if (*(p+1) == quote)
                p++;
            else {
                if (*(p+1) == '\\')
                    *q++ = *p++;
            }
        }
        *q++ = *p++;
    }
    if (*p == '\0')
        yyerror ("string ran past end of line", "");
    else
        p++;
    *q = 0;
    return p;
}
