//Version 5.03 added <p> before table to seperate end dococument from home icon or webmaster
//version 5.02 save  stytle=textalign=left (seems that even with class=mmmm td style wins
//          give up on onMouseOver="Cursor:...
//      status gets Real-World-Systems  Not default http://www.real..
//  4/19/20 changed http to https to prevent 302's
document.write( 
"<style media=print>   td.screen {visibility:hidden}  </style>	"+
"<style media=screen>  td.screen {visibility:visible} </style>  "+
"<p>"+
"<table width=100% border=0 cellpadding=0 cellspacing=0 style=border-style:none>" );
if (document.url != "index.html" ) document.write (
" <tr> 								"+
"  <td class=screen style=text-align:left;border-style:none; colspan=2	>	"+ 
"<a href=/                                              "+
"	onLoad=\"status='Real-World-Systems.com' ; docuument.cursor=\'help\'; return true;\" 		"+
"       title=' Real-World-Systems.com '	>		"+
" <img src=https://www.Real-World-Systems.com/home.gif border=0 alt= '\r Real-World-Systems.com \r' >"+
"</a>");

document.write( 
"<tr>							"+
" <td class=screen style=text-align:left;border-style:none;>"+
"<a href=mailto:webmaster&#x40;Real-World-Systems.com 	"+
"  style=cursor:help;font-size:x-small;text-decoration:none; "+
"  title=' Questions about this web page '>webmaster</a> 	"+
" <td align=right valign=bottom style=border-style:none;border-color:red;border-width:0;font-family:verdana;font-size:xx-small; class=none>	"+
"<span style=color:whiteSmoke>" +
" This web page hand crafted by Dennis German        " +
"This page last modified on " + document.lastModified + "<br>	" +
"&copy;2021 Real-World-Systems, All rights reserved.		        	" +
"</span>"); 
var theWidth= screen.width;
if (window.innerWidth)                          theWidth = window.innerWidth;
 else if (document.documentElement && 
          document.documentElement.clientWidth) theWidth = document.documentElement.clientWidth; 
 else if (document.body )                       theWidth = document.body.clientWidth;
var k;
( document.referrer == "" ) ? referr="" : referr="&referrer="+ escape(document.referrer);

k =     document.location + referr +
  	"&size="    + theWidth +   
        "&"+document.cookie;  

document.write(
"<img src=\"https://www.Real-World-Systems.com/cgi-bin/logit.cgi?" + k + "\" width=300 height=3 border=0 >"+
"</table> ") 

// //////////// end of enddoc.js
