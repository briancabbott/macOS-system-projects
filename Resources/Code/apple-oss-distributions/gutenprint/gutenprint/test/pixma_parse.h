/******************************************************************************
 * pixma_parse.h parser for Canon BJL printjobs
 * Copyright (c) 2005 - 2007 Sascha Sommer <saschasommer@freenet.de>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *****************************************************************************/


#ifndef PIXMA_PARSE_H
#define PIXMA_PARSE_H 1


#define bswap_32(x) \
     ((((x) & 0xff000000) >> 24) | (((x) & 0x00ff0000) >>  8) | \
      (((x) & 0x0000ff00) <<  8) | (((x) & 0x000000ff) << 24))

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define be2me_32(x) bswap_32(x)
#else
#define be2me_32
#endif



/* Bitstream reader from FFmpeg (http://www.ffmpeg.org)
 * libavcodec/bitstream.h
 * 
 */

typedef struct PutBitContext {
    uint32_t bit_buf;
    int bit_left;
    uint8_t *buf, *buf_ptr, *buf_end;
} PutBitContext;

static inline void init_put_bits(PutBitContext *s, uint8_t *buffer, int buffer_size)
{
    s->buf = buffer;
    s->buf_end = s->buf + buffer_size;
    s->buf_ptr = s->buf;
    s->bit_left=32;
    s->bit_buf=0;
}

static inline void put_bits(PutBitContext *s, int n, unsigned int value)
{
    unsigned int bit_buf;
    int bit_left;

    /*    printf("put_bits=%d %x\n", n, value); */

    
    bit_buf = s->bit_buf;
    bit_left = s->bit_left;

    /*    printf("n=%d value=%x cnt=%d buf=%x\n", n, value, bit_cnt, bit_buf); */
    /* XXX: optimize */
    if (n < bit_left) {
        bit_buf = (bit_buf<<n) | value;
        bit_left-=n;
    } else {
	bit_buf<<=bit_left;
        bit_buf |= value >> (n - bit_left);
        *(uint32_t *)s->buf_ptr = be2me_32(bit_buf);
        /* printf("bitbuf = %08x\n", bit_buf); */
        s->buf_ptr+=4;
	bit_left+=32 - n;
        bit_buf = value;
    }

    s->bit_buf = bit_buf;
    s->bit_left = bit_left;
}

/* bit input */
/* buffer, buffer_end and size_in_bits must be present and used by every reader */
typedef struct GetBitContext {
    const uint8_t *buffer, *buffer_end;
    uint32_t *buffer_ptr;
    uint32_t cache0;
    uint32_t cache1;
    int bit_count;
    int size_in_bits;
} GetBitContext;


#    define NEG_SSR32(a,s) ((( int32_t)(a))>>(32-(s)))
#    define NEG_USR32(a,s) (((uint32_t)(a))>>(32-(s)))


#   define MIN_CACHE_BITS 32

#   define OPEN_READER(name, gb)\
        int name##_bit_count=(gb)->bit_count;\
        uint32_t name##_cache0= (gb)->cache0;\
        uint32_t name##_cache1= (gb)->cache1;\
        uint32_t * name##_buffer_ptr=(gb)->buffer_ptr;\

#   define CLOSE_READER(name, gb)\
        (gb)->bit_count= name##_bit_count;\
        (gb)->cache0= name##_cache0;\
        (gb)->cache1= name##_cache1;\
        (gb)->buffer_ptr= name##_buffer_ptr;\

#   define UPDATE_CACHE(name, gb)\
    if(name##_bit_count > 0){\
        const uint32_t next= be2me_32( *name##_buffer_ptr );\
        name##_cache0 |= NEG_USR32(next,name##_bit_count);\
        name##_cache1 |= next<<name##_bit_count;\
        name##_buffer_ptr++;\
        name##_bit_count-= 32;\
    }\

#   define SKIP_CACHE(name, gb, num)\
        name##_cache0 <<= (num);\
        name##_cache0 |= NEG_USR32(name##_cache1,num);\
        name##_cache1 <<= (num);

#   define SKIP_COUNTER(name, gb, num)\
        name##_bit_count += (num);\

#   define SKIP_BITS(name, gb, num)\
        {\
            SKIP_CACHE(name, gb, num)\
            SKIP_COUNTER(name, gb, num)\
        }\

#   define LAST_SKIP_BITS(name, gb, num) SKIP_BITS(name, gb, num)
#   define LAST_SKIP_CACHE(name, gb, num) SKIP_CACHE(name, gb, num)

#   define SHOW_UBITS(name, gb, num)\
        NEG_USR32(name##_cache0, num)

#   define SHOW_SBITS(name, gb, num)\
        NEG_SSR32(name##_cache0, num)

#   define GET_CACHE(name, gb)\
        (name##_cache0)


/**
 * reads 0-17 bits.
 * Note, the alt bitstream reader can read up to 25 bits, but the libmpeg2 reader can't
 */
static inline unsigned int get_bits(GetBitContext *s, int n){
    register int tmp;
    OPEN_READER(re, s)
    UPDATE_CACHE(re, s)
    tmp= SHOW_UBITS(re, s, n);
    LAST_SKIP_BITS(re, s, n)
    CLOSE_READER(re, s)
    return tmp;
}

/**
 * init GetBitContext.
 * @param buffer bitstream buffer, must be FF_INPUT_BUFFER_PADDING_SIZE bytes larger then the actual read bits
 * because some optimized bitstream readers read 32 or 64 bit at once and could read over the end
 * @param bit_size the size of the buffer in bits
 */
static inline void init_get_bits(GetBitContext *s,
                   uint8_t *buffer, int bit_size)
{
    const int buffer_size= (bit_size+7)>>3;

    s->buffer= buffer;
    s->size_in_bits= bit_size;
    s->buffer_end= buffer + buffer_size;
    s->buffer_ptr = (uint32_t*)buffer;
    s->bit_count = 32;
    s->cache0 = 0;
    s->cache1 = 0;

    {
        OPEN_READER(re, s)
        UPDATE_CACHE(re, s)
        UPDATE_CACHE(re, s)
        CLOSE_READER(re, s)
    }
    s->cache1 = 0;

}

/* 10to8 decompression table */
static const unsigned short Table8[] = {
    0x0,0x1,0x2,0x4,0x5,0x6,0x8,0x9,0xa,0x10,0x11,0x12,0x14,0x15,0x16,
    0x18,0x19,0x1a,0x20,0x21,0x22,0x24,0x25,0x26,0x28,0x29,0x2a,0x40,0x41,0x42,
    0x44,0x45,0x46,0x48,0x49,0x4a,0x50,0x51,0x52,0x54,0x55,0x56,0x58,0x59,0x5a,
    0x60,0x61,0x62,0x64,0x65,0x66,0x68,0x69,0x6a,0x80,0x81,0x82,0x84,0x85,0x86,
    0x88,0x89,0x8a,0x90,0x91,0x92,0x94,0x95,0x96,0x98,0x99,0x9a,0xa0,0xa1,0xa2,
    0xa4,0xa5,0xa6,0xa8,0xa9,0xaa,0x100,0x101,0x102,0x104,0x105,0x106,0x108,0x109,0x10a,
    0x110,0x111,0x112,0x114,0x115,0x116,0x118,0x119,0x11a,0x120,0x121,0x122,0x124,0x125,0x126,
    0x128,0x129,0x12a,0x140,0x141,0x142,0x144,0x145,0x146,0x148,0x149,0x14a,0x150,0x151,0x152,
    0x154,0x155,0x156,0x158,0x159,0x15a,0x160,0x161,0x162,0x164,0x165,0x166,0x168,0x169,0x16a,
    0x180,0x181,0x182,0x184,0x185,0x186,0x188,0x189,0x18a,0x190,0x191,0x192,0x194,0x195,0x196,
    0x198,0x199,0x19a,0x1a0,0x1a1,0x1a2,0x1a4,0x1a5,0x1a6,0x1a8,0x1a9,0x1aa,0x200,0x201,0x202,
    0x204,0x205,0x206,0x208,0x209,0x20a,0x210,0x211,0x212,0x214,0x215,0x216,0x218,0x219,0x21a,
    0x220,0x221,0x222,0x224,0x225,0x226,0x228,0x229,0x22a,0x240,0x241,0x242,0x244,0x245,0x246,
    0x248,0x249,0x24a,0x250,0x251,0x252,0x254,0x255,0x256,0x258,0x259,0x25a,0x260,0x261,0x262,
    0x264,0x265,0x266,0x268,0x269,0x26a,0x280,0x281,0x282,0x284,0x285,0x286,0x288,0x289,0x28a,
    0x290,0x291,0x292,0x294,0x295,0x296,0x298,0x299,0x29a,0x2a0,0x2a1,0x2a2,0x2a4,0x2a5,0x2a6,
    0x2a8,0x2a9,0x2aa,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0
};


typedef struct rasterline_s {
	unsigned char* buf;
	unsigned int len;
	unsigned int line;
	struct rasterline_s* next;
} rasterline_t;


typedef struct color_s {
	char name;          /* name (one of CMYKcmyk) */
	int bpp;            /* number of bits */
	int level;          /* number of levels */
        int density;     /* relative density to the other colors*/
	unsigned int  value;/* last used dot value */
	unsigned int* dots;  /* number of dots for every level */
	int compression;    /* bits are compressed */
	rasterline_t* head; /* end of linked list */
	rasterline_t* tail; /* start of linked list */
	rasterline_t* pos;  /* iterator position */
} color_t;


typedef struct image_s {
        unsigned int width;
        unsigned int height;
        unsigned int dots;
	unsigned int image_top;
	unsigned int image_bottom;
	unsigned int image_left;
	unsigned int image_right;
	float top;
	float left;
	int xres,yres;
        int y;
        color_t color[MAX_COLORS];
        char* color_order;
        int num_colors;
        int cur_color;
        int lines_per_block;
} image_t;

/* FIXME what are the 0xa3 and 0xad for? they are used in the PIXMA iP4200 CD mode */
static const unsigned char valid_colors[] = {'C','M','Y','K','c','m','y','k',0xa3,0xad}; 
#endif


