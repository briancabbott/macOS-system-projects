/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/*
 * Top-level parsing of gpr code into blocks
 *
 */

#include <ast.h>
#include <sfstr.h>
#include <error.h>
#include <parse.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static int lineno = 1;     /* current line number */
static int startLine = 1;  /* set to start line of bracketd content */
static int kwLine = 1;     /* set to line of keyword */

static char* case_str [] = {
  "BEGIN",
  "END",
  "BEG_G",
  "END_G",
  "N",
  "E",
  "EOF",
  "ERROR",
};

/* caseStr:
 * Convert case_t to string.
 */
static char*
caseStr (case_t cs)
{
  return case_str[(int)cs];
}

/* readc:
 * return character from input stream
 * while keeping track of line number.
 * Strip out comments, and return space or newline.
 * If a newline is seen in comment and ostr
 * is non-null, add newline to ostr.
 */
static int
readc (Sfio_t* str, Sfio_t* ostr)
{
  int     c;
  int     cc;
  
  switch (c = sfgetc(str)) {
  case '\n' :
    lineno++;
    break;
  case '/' :
    cc = sfgetc(str);
    switch (cc) {
    case '*' :             /* in C comment   */
      while (1) {
        switch (c = sfgetc(str)) {
        case '\n' :
          lineno++;
          if (ostr) sfputc (ostr, c);
          break;
        case '*':
          switch (cc = sfgetc(str)) {
          case -1 :
            return cc;
            break;
          case '\n':
            lineno++;
            if (ostr) sfputc (ostr, cc);
            break;
          case '*':
            sfungetc(str, cc);
            break;
          case '/':
            return ' ';
            break;
          }
        }
      }
      break;
    case '/' :             /* in C++ comment */
      while ((c = sfgetc(str)) != '\n') {
        if (c < 0) return c;
      }
      lineno++;
      if (ostr) sfputc (ostr, c);
      break;
    default :              /* not a comment  */
      if (cc >= '\0') sfungetc (str, cc);
      break;
    }
    break;
  }
  return c;
}

/* unreadc;
 * push character back onto stream;
 * if newline, reduce lineno.
 */ 
void
unreadc (Sfio_t* str, int c)
{
  sfungetc (str, c);
  if (c  == '\n') lineno--;
}

/* skipWS:
 */
static int
skipWS (Sfio_t* str)
{
  int     c;

  while (1) {
    c = readc(str,0);
    if (!isspace(c)) {
      return c;
    }
  }
}

/* parseID:
 * Put initial alpha in buffer;
 * add additional alphas, up to buffer size.
 */
static void
parseID (Sfio_t* str, int c, char* buf, size_t bsize)
{
  int     more = 1;
  char*   ptr = buf;
  char*   eptr = buf + (bsize-1);

  *ptr++ = c;
  while (more) {
    c = readc (str,0);
    if (c < 0) more = 0;
    if (isalpha(c)||(c == '_')) {
      if (ptr == eptr) more = 0;
      else *ptr++ = c;
    }
    else {
      more = 0;
      unreadc (str, c);
    }
  }
  *ptr = '\0';
  
}

#define BSIZE 8

/* parseKind:
 * Look for keywords: BEGIN, END, BEG_G, END_G, N, E
 * As side-effect, sets kwLine to line of keyword.
 */
static case_t
parseKind (Sfio_t* str)
{
  int     c;
  char    buf[BSIZE];
  case_t  cs = Error;

  c = skipWS (str);
  if (c < 0) return Eof;
  if (!isalpha(c))
    error (ERROR_FATAL, "expected keyword BEGIN/END/N/E...; found '%c', line %d", c, lineno);

  kwLine = lineno;
  parseID (str, c, buf, BSIZE);
  switch (c) {
    case 'B' :
      if (strcmp(buf, "BEGIN") == 0) cs = Begin;
      if (strcmp(buf, "BEG_G") == 0) cs = BeginG;
      break;
    case 'E' :
      if (buf[1] == '\0') cs = Edge;
      if (strcmp(buf, "END") == 0) cs = End;
      if (strcmp(buf, "END_G") == 0) cs = EndG;
      break;
    case 'N' :
      if (buf[1] == '\0') cs = Node;
      break;
  }
  if (cs == Error)
    error (ERROR_FATAL, "unexpected keyword \"%s\", line %d", buf, kwLine);
  return cs;
}

/* endString:
 * eat characters from ins, putting them into outs,
 * up to and including a terminating character ec
 * that is not escaped with a back quote.
 */
static void
endString (Sfio_t* ins, Sfio_t* outs, char ec)
{
  int sline = lineno;
  int c;

  while ((c = sfgetc(ins)) != ec) {
    if (c == '\\') {
      sfputc (outs, c);
      c = sfgetc(ins);
    }
    if (c < 0) 
      error (ERROR_FATAL, "unclosed string, start line %d", sline);
    if (c == '\n') lineno++;
    sfputc (outs, (char)c);
  }
  sfputc (outs, c);
}

/* endBracket:
 * eat characters from ins, putting them into outs,
 * up to a terminating character ec.
 * Strings are treated as atomic units: any ec in them
 * is ignored. Since matching bc-ec pairs might nest,
 * the function is called recursively.
 */
static int
endBracket (Sfio_t* ins, Sfio_t* outs, char bc, char ec)
{
  int c;

  while (1) {
    c = readc (ins,outs);
    if ((c < 0) || (c == ec)) return c;
    else if (c == bc) {
      sfputc (outs, (char)c);
      c = endBracket (ins, outs, bc, ec);
      if (c < 0) return c;
      else sfputc (outs, (char)c);
    }
    else if ((c == '\'') || (c == '"')) {
      sfputc (outs, (char)c);
      endString (ins, outs, c);
    }
    else sfputc (outs, (char)c);
  }
}
/* parseBracket:
 *  parse paired expression : bc <string> ec
 *  returning <string>
 * As a side-effect, set startLine to beginning of content.
 */
static char*
parseBracket (Sfio_t* str, Sfio_t* buf, int bc, int ec)
{
  int       c;

  c = skipWS (str);
  if (c < 0) return 0;
  if (c != bc) {
    unreadc (str, c);
    return 0;
  }
  startLine = lineno;
  c = endBracket (str, buf, bc, ec);
  if (c < 0) error (ERROR_FATAL, "unclosed bracket %c%c expression, start line %d",
    bc,ec,startLine);
  return strdup (sfstruse (buf));
}

/* parseAction:
 */
static char*
parseAction (Sfio_t* str, Sfio_t* buf)
{
   return parseBracket(str, buf, '{', '}');
}

/* parseGuard:
 */
static char*
parseGuard (Sfio_t* str, Sfio_t* buf)
{
   return parseBracket(str, buf, '[', ']');
}

/* parseCase:
 * Recognize
 *   BEGIN <optional action>
 *   END <optional action>
 *   BEG_G <optional action>
 *   END_G <optional action>
 *   N <optional guard> <optional action>
 *   E <optional guard> <optional action>
 * where
 *   guard = '[' <expr> ']'
 *   action = '{' <expr> '}'
 */
static case_t
parseCase (Sfio_t* str, char** guard, int* gline, char** action, int* aline)
{
  case_t  kind;
  Sfio_t* buf = sfstropen ();

  kind = parseKind (str);
  switch (kind) {
    case Begin :
    case BeginG :
    case End :
    case EndG :
      *action = parseAction (str, buf);
      *aline = startLine;
      break;
    case Edge :
    case Node :
      *guard = parseGuard (str, buf);
      *gline = startLine;
      *action = parseAction (str, buf);
      *aline = startLine;
      break;
    case Eof :
    case Error :   /* to silence warnings */
      break;
  }

  sfstrclose (buf);
  return kind;
}

/* addCase:
 * create new case_info and append to list;
 * return new item as tail
 */
static case_info*
addCase (case_info* last, char* guard, int gline, char* action, int line, int* cnt)
{
  case_info* item;

  if (!guard && !action) {
    error (ERROR_WARNING, "Case with neither guard nor action, line %d - ignored", kwLine);
    return last;
  }

  *cnt = (*cnt)+1;
  item = newof(0,case_info,1,0);
  item->guard = guard;
  item->action = action;
  item->next = 0;
  if (guard) item->gstart = gline;
  if (action) item->astart = line;

  if (last)
    last->next = item;

  return item;
}

/* bindAction:
 *
 */
static void
bindAction (case_t cs, char* action, int aline, char** ap, int* lp)
{
  if (!action)
    error (ERROR_WARNING, "%s with no action, line %d - ignored", caseStr(cs), kwLine);
  else if (*ap) 
    error (ERROR_FATAL, "additional %s section, line %d", caseStr(cs), kwLine);
  else {
    *ap = action;
    *lp = aline;
  }
}

/* parseProg:
 * Parses input into gpr sections.
 * Only returns if successful.
 */
parse_prog *
parseProg (char* input, int isFile)
{
  parse_prog* prog;
  Sfio_t*     str;
  char*       mode;
  char*       guard;
  char*       action;
  int         more;
  case_info*  edgelist = 0;
  case_info*  nodelist = 0;
  case_info*  edgel = 0;
  case_info*  nodel = 0;
  int         n_nstmts = 0;
  int         n_estmts = 0;
  int         line, gline;

  prog = newof(0,parse_prog,1,0);
  if (!prog) {
    error (ERROR_FATAL, "parseProg: out of memory");
  }
 
  if (isFile) {
    mode = "r";
    prog->source = input;
  }
  else {
    mode = "rs";
    prog->source = 0;   /* command line */
  }

  str = sfopen (0, input, mode);
  if (!str) { 
    if (isFile)
      error (ERROR_FATAL, "could not open %s for reading", input);
    else
      error (ERROR_FATAL, "parseProg : unable to create sfio stream");
  }

  more = 1;
  while (more) {
    switch (parseCase (str, &guard, &gline, &action, &line)) {
      case Begin :
        bindAction (Begin, action, line, &(prog->begin_stmt), &(prog->l_begin));
        break;
      case BeginG :
        bindAction (BeginG, action, line, &(prog->begg_stmt),&(prog->l_beging));
        break;
      case End :
        bindAction (End, action, line, &(prog->end_stmt),&(prog->l_end));
        break;
      case EndG :
        bindAction (EndG, action, line, &(prog->endg_stmt),&(prog->l_endg));
        break;
      case Eof :
        more = 0;
        break;
      case Node :
        nodel = addCase (nodel, guard, gline, action, line, &n_nstmts);
        if (!nodelist) nodelist = nodel;
        break;
      case Edge :
        edgel = addCase (edgel, guard, gline, action, line, &n_estmts);
        if (!edgelist) edgelist = edgel;
        break;
      case Error :  /* to silence warnings */
        break;
    }
  }

  prog->node_stmts = nodelist;
  prog->edge_stmts = edgelist;
  prog->n_nstmts = n_nstmts;
  prog->n_estmts = n_estmts;

  sfclose (str);

  return prog;
}

