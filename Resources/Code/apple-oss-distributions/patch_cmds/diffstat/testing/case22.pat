--- pgp-menu-traditional/PATCHES Dec 2002 17:44:54 -0000	3.6
+++ pgp-menu-traditional/PATCHES Feb 2003 11:26:39 -0000
@@ -0,0 +1 @@
+patch-1.5.3.dw.pgp-menu-traditional.1
--- pgp-menu-traditional/compose.c Dec 2002 11:19:39 -0000	3.8
+++ pgp-menu-traditional/compose.c Feb 2003 11:26:42 -0000
@@ -145,2 +145,6 @@ static void redraw_crypt_lines (HEADER *
     addstr (_("Clear"));
+#ifdef HAVE_PGP
+  if ((msg->security & PGPINLINE) == PGPINLINE)
+    addstr (_(" (inline)"));
+#endif
   clrtoeol ();
@@ -174,4 +178,4 @@ static int pgp_send_menu (HEADER *msg, i
 
-  switch (mutt_multi_choice (_("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "),
-			     _("esabf")))
+  switch (mutt_multi_choice (_("PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline or (f)orget it? "),
+			     _("esabtf")))
   {
@@ -208,6 +212,11 @@ static int pgp_send_menu (HEADER *msg, i
   case 4: /* (b)oth */
-    msg->security = PGPENCRYPT | PGPSIGN;
+    msg->security |= PGPENCRYPT | PGPSIGN;
     break;
 
-  case 5: /* (f)orget it */
+  case 5: /* (t)raditional inline */
+    if (msg->security & (ENCRYPT | SIGN))
+	msg->security |= PGPINLINE;		/* should this be a toggle instead */
+    break;
+
+  case 6: /* (f)orget it */
     msg->security = 0;
--- pgp-menu-traditional/crypt.c Dec 2002 22:23:42 -0000	3.11
+++ pgp-menu-traditional/crypt.c Feb 2003 11:26:43 -0000
@@ -172,3 +172,2 @@ int mutt_protect (HEADER *msg, char *key
   BODY *tmp_pgp_pbody = NULL;
-  int traditional = 0;
   int flags = msg->security, i;
@@ -179,18 +178,9 @@ int mutt_protect (HEADER *msg, char *key
 #ifdef HAVE_PGP
-  if (msg->security & APPLICATION_PGP)
+  if ((msg->security & PGPINLINE) == PGPINLINE)
   {
-    if ((msg->content->type == TYPETEXT) &&
-	!ascii_strcasecmp (msg->content->subtype, "plain"))
-    {
-      if ((i = query_quadoption (OPT_PGPTRADITIONAL, _("Create an inline PGP message?"))) == -1)
-	return -1;
-      else if (i == M_YES)
-	traditional = 1;
-    }
-    if (traditional)
+    /* they really want to send it inline... go for it */
+    if (!isendwin ()) mutt_endwin _("Invoking PGP...");
+    pbody = pgp_traditional_encryptsign (msg->content, flags, keylist);
+    if (pbody)
     {
-      if (!isendwin ()) mutt_endwin _("Invoking PGP...");
-      if (!(pbody = pgp_traditional_encryptsign (msg->content, flags, keylist)))
-	return -1;
-
       msg->content = pbody;
@@ -198,2 +188,8 @@ int mutt_protect (HEADER *msg, char *key
     }
+
+    /* otherwise inline won't work...ask for revert */
+    if ((i = query_quadoption (OPT_PGPMIMEASK, _("Message can't be sent inline.  Revert to using PGP/MIME?"))) != M_YES)
+      return -1;
+
+    /* go ahead with PGP/MIME */
   }
--- pgp-menu-traditional/init.h Dec 2002 18:09:49 -0000	3.28
+++ pgp-menu-traditional/init.h Feb 2003 11:26:48 -0000
@@ -1352,2 +1352,44 @@ struct option_t MuttVars[] = {
   */
+  { "pgp_create_traditional",	DT_SYN, R_NONE, UL "pgp_autoinline", 0 },
+  { "pgp_autoinline",		DT_BOOL, R_NONE, OPTPGPAUTOINLINE, 0 },
+  /*
+  ** .pp
+  ** This option controls whether Mutt generates old-style inline
+  ** (traditional) PGP encrypted or signed messages under certain
+  ** circumstances.  This can be overridden by use of the \fIpgp-menu\fP,
+  ** when inline is not required.
+  ** .pp
+  ** Note that Mutt might automatically use PGP/MIME for messages
+  ** which consist of more than a single MIME part.  Mutt can be
+  ** configured to ask before sending PGP/MIME messages when inline
+  ** (traditional) would not work.
+  ** See also: ``$$pgp_mime_ask''.
+  ** .pp
+  ** Also note that using the old-style PGP message format is \fBstrongly\fP
+  ** \fBdeprecated\fP.
+  ** (PGP only)
+  */
+  { "pgp_auto_traditional",	DT_SYN, R_NONE, UL "pgp_replyinline", 0 },
+  { "pgp_replyinline",		DT_BOOL, R_NONE, OPTPGPREPLYINLINE, 0 },
+  /*
+  ** .pp
+  ** Setting this variable will cause Mutt to always attempt to
+  ** create an inline (traditional) message when replying to a
+  ** message which is PGP encrypted/signed inline.  This can be
+  ** overridden by use of the \fIpgp-menu\fP, when inline is not
+  ** required.  This option does not automatically detect if the
+  ** (replied-to) message is inline; instead it relies on Mutt
+  ** internals for previously checked/flagged messages.
+  ** .pp
+  ** Note that Mutt might automatically use PGP/MIME for messages
+  ** which consist of more than a single MIME part.  Mutt can be
+  ** configured to ask before sending PGP/MIME messages when inline
+  ** (traditional) would not work.
+  ** See also: ``$$pgp_mime_ask''.
+  ** .pp
+  ** Also note that using the old-style PGP message format is \fBstrongly\fP
+  ** \fBdeprecated\fP.
+  ** (PGP only)
+  ** 
+  */
   { "pgp_show_unusable", DT_BOOL, R_NONE, OPTPGPSHOWUNUSABLE, 1 },
@@ -1396,11 +1438,8 @@ struct option_t MuttVars[] = {
   */
-  { "pgp_create_traditional", DT_QUAD, R_NONE, OPT_PGPTRADITIONAL, M_NO },
+  { "pgp_mime_ask", DT_QUAD, R_NONE, OPT_PGPMIMEASK, M_NO },
   /*
   ** .pp
-  ** This option controls whether Mutt generates old-style PGP encrypted
-  ** or signed messages under certain circumstances.
-  ** .pp
-  ** Note that PGP/MIME will be used automatically for messages which have
-  ** a character set different from us-ascii, or which consist of more than
-  ** a single MIME part.
+  ** This option controls whether Mutt will prompt you for
+  ** automatically sending a (signed/encrypted) message using
+  ** PGP/MIME when inline (traditional) fails (for any reason).
   ** .pp
@@ -1409,2 +1448,3 @@ struct option_t MuttVars[] = {
   */
+
 
--- pgp-menu-traditional/mutt.h Dec 2002 08:53:21 -0000	3.12
+++ pgp-menu-traditional/mutt.h Feb 2003 11:26:51 -0000
@@ -270,3 +270,3 @@ enum
 #ifdef HAVE_PGP
-  OPT_PGPTRADITIONAL, /* create old-style PGP messages */
+  OPT_PGPMIMEASK,     /* ask to revert to PGP/MIME when inline fails */
 #endif
@@ -447,2 +447,4 @@ enum
   OPTPGPLONGIDS,
+  OPTPGPAUTOINLINE,
+  OPTPGPREPLYINLINE,
 #endif
--- pgp-menu-traditional/pgp.c Dec 2002 17:59:51 -0000	3.18
+++ pgp-menu-traditional/pgp.c Feb 2003 11:26:55 -0000
@@ -535,2 +535,5 @@ int mutt_is_application_pgp (BODY *m)
   }
+  if (t)
+    t |= PGPINLINE;
+
   return t;
@@ -1057,3 +1060,3 @@ char *pgp_findKeys (ADDRESS *to, ADDRESS
   int i;
-  pgp_key_t *k_info, *key;
+  pgp_key_t *k_info, *key = NULL;
 
--- pgp-menu-traditional/pgplib.h Dec 2002 11:19:40 -0000	3.3
+++ pgp-menu-traditional/pgplib.h Feb 2003 11:26:56 -0000
@@ -27,2 +27,3 @@
 #define PGPKEY      (APPLICATION_PGP | (1 << 3))
+#define PGPINLINE   (APPLICATION_PGP | (1 << 4))
 
--- pgp-menu-traditional/postpone.c Dec 2002 11:19:40 -0000	3.7
+++ pgp-menu-traditional/postpone.c Feb 2003 11:26:57 -0000
@@ -492,2 +492,9 @@ int mutt_parse_crypt_hdr (char *p, int s
 
+      case 'i':
+      case 'I':
+#ifdef HAVE_PGP
+	pgp |= (PGPINLINE & ~APPLICATION_PGP);
+#endif
+	break;
+
       default:
--- pgp-menu-traditional/send.c Dec 2002 22:47:57 -0000	3.15
+++ pgp-menu-traditional/send.c Feb 2003 11:27:01 -0000
@@ -1259,3 +1259,3 @@ ci_send_message (int flags,		/* send mod
 	msg->security |= SIGN;
-    }      
+    }
 
@@ -1279,2 +1279,8 @@ ci_send_message (int flags,		/* send mod
 	msg->security |= APPLICATION_PGP;
+      /*
+       * we leave this so late because PGPINLINE should be applied only when APPLICATION_PGP is high
+       * perhaps reserving a bit in crypt.h would be more reasonable, though it doesn't apply with S/MIME
+       */
+      if (option (OPTPGPREPLYINLINE) && (cur->security & PGPINLINE) == PGPINLINE)
+	msg->security |= PGPINLINE;
 #endif /* HAVE_PGP */
@@ -1294,2 +1300,10 @@ ci_send_message (int flags,		/* send mod
     }
+#ifdef HAVE_PGP
+    /*
+     * we leave this so late because PGPINLINE should be applied only when APPLICATION_PGP is high
+     * perhaps reserving a bit in crypt.h would be more reasonable, though it doesn't apply with S/MIME
+     */
+    if ((msg->security & APPLICATION_PGP) && (option (OPTPGPAUTOINLINE)))
+      msg->security |= PGPINLINE;
+#endif
 #endif /* HAVE_PGP || HAVE_SMIME */
--- pgp-menu-traditional/sendlib.c Dec 2002 20:56:48 -0000	3.18
+++ pgp-menu-traditional/sendlib.c Feb 2003 11:27:02 -0000
@@ -2429,2 +2429,4 @@ int mutt_write_fcc (const char *path, HE
     }
+    if ((hdr->security & PGPINLINE) == PGPINLINE)
+      fputc ('I', msg->fp);
     fputc ('\n', msg->fp);
--- pgp-menu-traditional/po/ca.po Dec 2002 10:37:21 -0000	3.7
+++ pgp-menu-traditional/po/ca.po Feb 2003 11:27:19 -0000
@@ -596,10 +596,10 @@ msgstr "Xifra"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "(x)ifra, (s)igna, s(i)gna com a, (a)mbd�s, o en (c)lar? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "(x)ifra, (s)igna, s(i)gna com a, (a)mbd�s, traditional en (l)�nia, o en (c)lar? "
 
 # ivb (2001/11/19)
-# ivb  (x)ifra, (s)igna, s(i)gna com a, (a)mbd�s, (c)lar
+# ivb  (x)ifra, (s)igna, s(i)gna com a, (a)mbd�s, traditional en (l)�nia, o en (c)lar
 #: compose.c:176
-msgid "esabf"
-msgstr "xsiac"
+msgid "esabtf"
+msgstr "xsialc"
 
--- pgp-menu-traditional/po/cs.po Dec 2002 10:37:21 -0000	3.6
+++ pgp-menu-traditional/po/cs.po Feb 2003 11:27:20 -0000
@@ -676,4 +676,4 @@ msgstr "Za�ifrovat"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "(�)ifrovat, (p)odepsat, podepsat (j)ako, (o)boj�, �i (n)ic?"
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "(�)ifrovat, (p)odepsat, podepsat (j)ako, (o)boj�, (t)raditional/p��m�, �i (n)ic?"
 
@@ -681,4 +681,4 @@ msgstr "(�)ifrovat, (p)odepsat, podepsat
 #: compose.c:176
-msgid "esabf"
-msgstr "�pjon"
+msgid "esabtf"
+msgstr "�pjotn"
 
--- pgp-menu-traditional/po/da.po Dec 2002 10:37:21 -0000	3.6
+++ pgp-menu-traditional/po/da.po Feb 2003 11:27:21 -0000
@@ -566,8 +566,8 @@ msgstr "Krypt�r"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "(k)rypt�r, (u)nderskriv, underskriv (s)om, (b)egge, (i)ngen PGP"
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "(k)rypt�r, (u)nderskriv, underskriv (s)om, (b)egge, (t)raditional integreret, (i)ngen PGP"
 
 #: compose.c:176
-msgid "esabf"
-msgstr "kusbi"
+msgid "esabtf"
+msgstr "kusbti"
 
--- pgp-menu-traditional/po/de.po Dec 2002 10:37:21 -0000	3.7
+++ pgp-menu-traditional/po/de.po Feb 2003 11:27:22 -0000
@@ -558,8 +558,8 @@ msgstr "Verschl�sseln mit: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (v)erschl., (s)ign., sign. (a)ls, (b)eides, (k)ein PGP? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (v)erschl., (s)ign., sign. (a)ls, (b)eides, (t)raditionelles Inline, (k)ein PGP? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "vsabk"
+msgid "esabtf"
+msgstr "vsabtk"
 
--- pgp-menu-traditional/po/el.po Dec 2002 10:37:21 -0000	3.7
+++ pgp-menu-traditional/po/el.po Feb 2003 11:27:24 -0000
@@ -690,4 +690,4 @@ msgstr "������������� ��: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, � (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional �������, � (f)orget it? "
 
@@ -696,4 +696,4 @@ msgstr "PGP (e)ncrypt, (s)ign, sign (a)s
 #: compose.c:176
-msgid "esabf"
-msgstr "esabf"
+msgid "esabtf"
+msgstr "esabtf"
 
--- pgp-menu-traditional/po/eo.po Dec 2002 10:37:21 -0000	3.6
+++ pgp-menu-traditional/po/eo.po Feb 2003 11:27:25 -0000
@@ -566,8 +566,8 @@ msgstr "�ifri"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "�(i)fri, (s)ubskribi, subskribi (k)iel, (a)mba�, a� (f)orgesi? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "�(i)fri, (s)ubskribi, subskribi (k)iel, (a)mba�, (t)raditional \"inline\", a� (f)orgesi? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "iskaf"
+msgid "esabtf"
+msgstr "iskatf"
 
--- pgp-menu-traditional/po/es.po Dec 2002 10:37:22 -0000	3.7
+++ pgp-menu-traditional/po/es.po Feb 2003 11:27:26 -0000
@@ -564,8 +564,9 @@ msgstr "Cifrar"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "�co(d)ificar, f(i)rmar (c)omo, amb(o)s o ca(n)celar? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "�co(d)ificar, f(i)rmar (c)omo, amb(o)s, (t)radicional incluido, o ca(n)celar? "
+
 
 #: compose.c:176
-msgid "esabf"
-msgstr "dicon"
+msgid "esabtf"
+msgstr "dicotn"
 
--- pgp-menu-traditional/po/et.po Dec 2002 10:37:22 -0000	3.8
+++ pgp-menu-traditional/po/et.po Feb 2003 11:27:27 -0000
@@ -559,7 +559,7 @@ msgstr "Kr�pti kasutades: "
 msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (k)r�pti, (a)llkiri, allk. ku(i), (m)�lemad v�i (u)nusta? "
+msgstr "PGP (k)r�pti, (a)llkiri, allk. ku(i), (m)�lemad, (t)raditional kehasse, v�i (u)nusta? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "kaimu"
+msgid "esabtf"
+msgstr "kaimtu"
 
--- pgp-menu-traditional/po/fr.po Dec 2002 10:37:22 -0000	3.13
+++ pgp-menu-traditional/po/fr.po Feb 2003 11:27:28 -0000
@@ -585,8 +585,8 @@ msgstr "Chiffrer avec : "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "(c)hiffrer PGP, (s)igner, (e)n tant que, les (d)eux, ou (o)ublier ? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "(c)hiffrer PGP, (s)igner, (e)n tant que, les (d)eux, (t)raditionnel en ligne, ou (o)ublier ? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "csedo"
+msgid "esabtf"
+msgstr "csedto"
 
--- pgp-menu-traditional/po/gl.po Dec 2002 10:37:22 -0000	3.6
+++ pgp-menu-traditional/po/gl.po Feb 2003 11:27:30 -0000
@@ -568,8 +568,8 @@ msgstr "Encriptar"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "�(e)ncriptar, (f)irmar, firmar (c)omo, (a)mbas ou (o)lvidar? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "�(e)ncriptar, (f)irmar, firmar (c)omo, (a)mbas, (t)raditional interior, ou (o)lvidar? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "efcao"
+msgid "esabtf"
+msgstr "efcato"
 
--- pgp-menu-traditional/po/hu.po Dec 2002 10:37:23 -0000	3.6
+++ pgp-menu-traditional/po/hu.po Feb 2003 11:27:31 -0000
@@ -569,8 +569,8 @@ msgstr "Titkos�t"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "(t)itkos�t, (a)l��r, al��r (m)int, titkos�t �(s) al��r, m�(g)se? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "(t)itkos�t, (a)l��r, al��r (m)int, titkos�t �(s) al��r, traditional (b)e�gyazott, m�(g)se? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "tamsg"
+msgid "esabtf"
+msgstr "tamsbg"
 
--- pgp-menu-traditional/po/id.po Dec 2002 10:37:23 -0000	3.7
+++ pgp-menu-traditional/po/id.po Feb 2003 11:27:33 -0000
@@ -562,8 +562,8 @@ msgstr "Enkrip dengan: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (e)nkrip, (t)andatangan, tandatangan (s)bg, ke(d)uanya, (b)atal? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (e)nkrip, (t)andatangan, tandatangan (s)bg, ke(d)uanya, traditional (i)nline, (b)atal? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "etsdb"
+msgid "esabtf"
+msgstr "etsdib"
 
--- pgp-menu-traditional/po/it.po Dec 2002 10:37:23 -0000	3.6
+++ pgp-menu-traditional/po/it.po Feb 2003 11:27:34 -0000
@@ -570,8 +570,8 @@ msgstr "Crittografa"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "cifra(e), firma(s), firma come(a), entrambi(b), annulla(f) "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "cifra(e), firma(s), firma come(a), entrambi(b), (t)radizionale in linea , annulla(f) "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "esabf"
+msgid "esabtf"
+msgstr "esabtf"
 
--- pgp-menu-traditional/po/ja.po Dec 2002 10:37:23 -0000	3.13
+++ pgp-menu-traditional/po/ja.po Feb 2003 11:27:35 -0000
@@ -558,8 +558,9 @@ msgstr "  �Ź沽����: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (e)�Ź沽,(s)��̾,(a)..�Ȥ��ƽ�̾,(b)ξ��,(f)���?"
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (e)�Ź沽,(s)��̾,(a)..�Ȥ��ƽ�̾,(b)ξ��,(i)nline,(f)���?"
+
 
 #: compose.c:176
-msgid "esabf"
-msgstr ""
+msgid "esabtf"
+msgstr "esabif"
 
--- pgp-menu-traditional/po/ko.po Dec 2002 10:37:23 -0000	3.9
+++ pgp-menu-traditional/po/ko.po Feb 2003 11:27:36 -0000
@@ -560,8 +560,8 @@ msgstr "��ȣȭ ���: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP ��ȣȭ(e), ����(s), ��� ����(a), �� ��(b), ���(f)? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP ��ȣȭ(e), ����(s), ��� ����(a), �� ��(b), (i)nline, ���(f)? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "esabf"
+msgid "esabtf"
+msgstr "esabif"
 
--- pgp-menu-traditional/po/lt.po Dec 2002 10:37:23 -0000	3.6
+++ pgp-menu-traditional/po/lt.po Feb 2003 11:27:38 -0000
@@ -566,5 +566,5 @@ msgstr "U��ifruoti"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr ""
-"(u)��ifruot, pa(s)ira�yt, pasira�yt k(a)ip, a(b)u, rinktis (m)ic algoritm�, "
+"(u)��ifruot, pa(s)ira�yt, pasira�yt k(a)ip, a(b)u, traditional (l)ai�ke, "
 "ar (p)amir�ti?"
@@ -573,4 +573,4 @@ msgstr ""
 #, fuzzy
-msgid "esabf"
-msgstr "usabmp"
+msgid "esabtf"
+msgstr "usablp"
 
@@ -586,4 +586,3 @@ msgid ""
 msgstr ""
-"(u)��ifruot, pa(s)ira�yt, pasira�yt k(a)ip, a(b)u, rinktis (m)ic algoritm�, "
-"ar (p)amir�ti?"
+"(u)��ifruot, pa(s)ira�yt, u��ifruo(t) su, pasira�yt k(a)ip, a(b)u, ar (p)amir�ti?"
 
@@ -592,3 +591,3 @@ msgstr ""
 msgid "eswabf"
-msgstr "usabmp"
+msgstr "ustabp"
 
--- pgp-menu-traditional/po/nl.po Dec 2002 10:37:23 -0000	3.7
+++ pgp-menu-traditional/po/nl.po Feb 2003 11:27:39 -0000
@@ -553,8 +553,8 @@ msgstr "Versleutelen met: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (v)ersleutel, (o)ndertekenen, ondert. (a)ls, (b)eide, (g)een? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (v)ersleutel, (o)ndertekenen, ondert. (a)ls, (b)eide, (t)raditioneel bericht, (g)een? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "voabg"
+msgid "esabtf"
+msgstr "voabtg"
 
--- pgp-menu-traditional/po/pl.po Dec 2002 10:37:23 -0000	3.8
+++ pgp-menu-traditional/po/pl.po Feb 2003 11:27:40 -0000
@@ -560,8 +560,8 @@ msgstr "Zaszyfruj u�ywaj�c: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (z)aszyfruj, podpi(s)z, podpisz j(a)ko, o(b)a, b(e)z PGP? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (z)aszyfruj, podpi(s)z, podpisz j(a)ko, o(b)a, (t)raditional inline, b(e)z PGP? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "zsabe"
+msgid "esabtf"
+msgstr "zsabte"
 
--- pgp-menu-traditional/po/pt_BR.po Dec 2002 10:37:23 -0000	3.7
+++ pgp-menu-traditional/po/pt_BR.po Feb 2003 11:27:41 -0000
@@ -569,5 +569,5 @@ msgstr "Encriptar"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr ""
-"(e)ncripa, a(s)sina, assina (c)omo, (a)mbos, escolhe (m)ic, ou es(q)uece? "
+"(e)ncripa, a(s)sina, assina (c)omo, (a)mbos, (t)raditional em (l)inha, ou es(q)uece? "
 
@@ -575,4 +575,4 @@ msgstr ""
 #, fuzzy
-msgid "esabf"
-msgstr "escamq"
+msgid "esabtf"
+msgstr "escalq"
 
@@ -588,3 +588,3 @@ msgid ""
 msgstr ""
-"(e)ncripa, a(s)sina, assina (c)omo, (a)mbos, escolhe (m)ic, ou es(q)uece? "
+"(e)ncripa, a(s)sina, e(n)cripa com, assina (c)omo, (a)mbos, ou es(q)uece? "
 
@@ -593,3 +593,3 @@ msgstr ""
 msgid "eswabf"
-msgstr "escamq"
+msgstr "esncaq"
 
--- pgp-menu-traditional/po/ru.po Dec 2002 10:37:24 -0000	3.10
+++ pgp-menu-traditional/po/ru.po Feb 2003 11:27:43 -0000
@@ -567,8 +567,8 @@ msgstr "�����������: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP (e)����, (s)�������, (a)������� ���, (b)���, (f)����������? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP (e)����, (s)�������, (a)������� ���, (b)���, traditional (i)nline, (f)����������? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr "esabf"
+msgid "esabtf"
+msgstr "esabif"
 
--- pgp-menu-traditional/po/sk.po Dec 2002 10:37:24 -0000	3.6
+++ pgp-menu-traditional/po/sk.po Feb 2003 11:27:44 -0000
@@ -574,5 +574,5 @@ msgstr "Za�ifruj"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr ""
-"(e)-�ifr, (s)-podp, podp (a)ko, o(b)e, ozna� alg. mi(c), alebo (f)-zabudn�� "
+"(e)-�ifr, (s)-podp, podp (a)ko, o(b)e, (t)raditional inline, alebo (f)-zabudn�� "
 "na to? "
@@ -581,4 +581,4 @@ msgstr ""
 #, fuzzy
-msgid "esabf"
-msgstr "esabmf"
+msgid "esabtf"
+msgstr "esabtf"
 
@@ -594,4 +594,3 @@ msgid ""
 msgstr ""
-"(e)-�ifr, (s)-podp, podp (a)ko, o(b)e, ozna� alg. mi(c), alebo (f)-zabudn�� "
-"na to? "
+"(e)-�ifr, (s)-podp, (w)-�ifr s, podp (a)ko, o(b)e, alebo (f)-zabudn�� na to? "
 
@@ -600,3 +599,3 @@ msgstr ""
 msgid "eswabf"
-msgstr "esabmf"
+msgstr "eswabf"
 
--- pgp-menu-traditional/po/sv.po Dec 2002 10:37:24 -0000	3.7
+++ pgp-menu-traditional/po/sv.po Feb 2003 11:27:45 -0000
@@ -555,8 +555,8 @@ msgstr "Kryptera med: "
 #: compose.c:175
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "PGP: (k)ryptera, (s)ignera, signera s(o)m, (b)�da, eller sk(i)ppa det?"
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "PGP: (k)ryptera, (s)ignera, signera s(o)m, (b)�da, (t)raditional infogat, eller sk(i)ppa det?"
 
 #: compose.c:176
-msgid "esabf"
-msgstr "ksobi"
+msgid "esabtf"
+msgstr "ksobti"
 
--- pgp-menu-traditional/po/tr.po Dec 2002 10:37:24 -0000	3.6
+++ pgp-menu-traditional/po/tr.po Feb 2003 11:27:47 -0000
@@ -565,5 +565,5 @@ msgstr "�ifrele"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr ""
-"�(i)frele, i(m)zala, (f)arkl� imzala, i(k)isi de, mi(c) algoritmini se� "
+"�(i)frele, i(m)zala, (f)arkl� imzala, i(k)isi de, (t)raditional inline, "
 "yoksa i(p)talm�? "
@@ -571,4 +571,4 @@ msgstr ""
 #: compose.c:176
-msgid "esabf"
-msgstr "imfkcp"
+msgid "esabtf"
+msgstr "imfktp"
 
--- pgp-menu-traditional/po/uk.po Dec 2002 10:37:24 -0000	3.7
+++ pgp-menu-traditional/po/uk.po Feb 2003 11:27:48 -0000
@@ -560,8 +560,8 @@ msgstr "����������"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
-msgstr "����.(e), Ц��.(s), Ц��. ��(a), ���(b) �� צ�ͦ��(f)? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
+msgstr "����.(e), Ц��.(s), Ц��. ��(a), ���(b), traditional (i)nline �� צ�ͦ��(f)? "
 
 #: compose.c:176
-msgid "esabf"
-msgstr ""
+msgid "esabtf"
+msgstr "esabif"
 
--- pgp-menu-traditional/po/zh_CN.po Dec 2002 10:37:24 -0000	3.6
+++ pgp-menu-traditional/po/zh_CN.po Feb 2003 11:27:50 -0000
@@ -573,5 +573,5 @@ msgstr "����"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr ""
-"(e)����, (s)ǩ��, (a)�ñ�����ǩ, (b)���߽�Ҫ, ѡ�� (m)ic ���㷨 �� (f)��"
+"(e)����, (s)ǩ��, (a)�ñ�����ǩ, (b)���߽�Ҫ, (t)raditional inline, �� (f)��"
 "����"
@@ -579,3 +579,3 @@ msgstr ""
 #: compose.c:176
-msgid "esabf"
+msgid "esabtf"
 msgstr ""
--- pgp-menu-traditional/po/zh_TW.po Dec 2002 10:37:24 -0000	3.6
+++ pgp-menu-traditional/po/zh_TW.po Feb 2003 11:27:51 -0000
@@ -565,3 +565,3 @@ msgstr "加密"
 #, fuzzy
-msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, or (f)orget it? "
+msgid "PGP (e)ncrypt, (s)ign, sign (a)s, (b)oth, (t)raditional inline, or (f)orget it? "
 msgstr "(1)加密, (2)簽名, (3)用別的身份簽, (4)兩者皆要, 或 (5)放棄？"
@@ -569,4 +569,4 @@ msgstr "(1)加密, (2)簽名, (3)用別�
 #: compose.c:176
-msgid "esabf"
-msgstr "12345"
+msgid "esabtf"
+msgstr "1234t5"
 
