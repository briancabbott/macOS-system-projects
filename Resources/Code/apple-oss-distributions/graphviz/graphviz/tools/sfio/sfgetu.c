#include	"sfhdr.h"

/*	Read an unsigned long value coded in a portable format.
**
**	Written by Kiem-Phong Vo
*/

#if __STD_C
Sfulong_t sfgetu(reg Sfio_t* f)
#else
Sfulong_t sfgetu(f)
reg Sfio_t*	f;
#endif
{
	Sfulong_t	v;
	reg uchar	*s, *ends, c;
	reg int		p;

	SFMTXSTART(f, (Sfulong_t)(-1));

	if(f->mode != SF_READ && _sfmode(f,SF_READ,0) < 0)
		SFMTXRETURN(f, (Sfulong_t)(-1));

	SFLOCK(f,0);

	for(v = 0;;)
	{	if(SFRPEEK(f,s,p) <= 0)
		{	f->flags |= SF_ERROR;
			v = (Sfulong_t)(-1);
			goto done;
		}
		for(ends = s+p; s < ends;)
		{	c = *s++;
			v = (v << SF_UBITS) | SFUVALUE(c);
			if(!(c&SF_MORE))
			{	f->next = s;
				goto done;
			}
		}
		f->next = s;
	}
done:
	SFOPEN(f,0);
	SFMTXRETURN(f, v);
}
