/* 
 * Testcase for bitmap
 */

#include "ompi_config.h"

#include <stdio.h>
#include "support.h"

#include "ompi/class/ompi_bitmap.h"
#include "ompi/constants.h"

#define BSIZE 26
#define SIZE_OF_CHAR (sizeof(char) * 8)
#define OMPI_INVALID_BIT -1
#define ERR_CODE -2

#define PRINT_VALID_ERR \
    fprintf(error_out, "================================ \n"); \
    fprintf(error_out, "This is suppossed to throw error \n"); \
    fprintf(error_out, "================================ \n")

static void test_bitmap_set(ompi_bitmap_t *bm);
static void test_bitmap_clear(ompi_bitmap_t *bm);
static void test_bitmap_is_set(ompi_bitmap_t *bm);
static void test_bitmap_clear_all(ompi_bitmap_t *bm);
static void test_bitmap_set_all(ompi_bitmap_t *bm);
static void test_bitmap_find_and_set(ompi_bitmap_t *bm);
static void test_bitmap_find_size(ompi_bitmap_t *bm);


static int set_bit(ompi_bitmap_t *bm, int bit);
static int clear_bit(ompi_bitmap_t *bm, int bit);
static int is_set_bit(ompi_bitmap_t *bm, int bit);
static int clear_all(ompi_bitmap_t *bm);
static int set_all(ompi_bitmap_t *bm);
static int find_and_set(ompi_bitmap_t *bm, int bit);
static int find_size(ompi_bitmap_t *bm);

#define WANT_PRINT_BITMAP 0
#if WANT_PRINT_BITMAP
static void print_bitmap(ompi_bitmap_t *bm);
#endif

static FILE *error_out=NULL;

int main(int argc, char *argv[])
{
    /* Local variables */
    ompi_bitmap_t bm;
    int err;
    
    /* Perform overall test initialization */
    test_init("ompi_bitmap_t");

#ifdef STANDALONE
    error_out = stderr;
#else
    error_out = fopen( "./ompi_bitmap_test_out.txt", "w" );
    if( error_out == NULL ) error_out = stderr;
#endif
    /* Initialize bitmap  */

    PRINT_VALID_ERR;
    err = ompi_bitmap_init(NULL, 2);
    if (err == OMPI_ERR_BAD_PARAM)
	fprintf(error_out, "ERROR: Initialization of bitmap failed\n\n");

    PRINT_VALID_ERR;
    err = ompi_bitmap_init(&bm, -1);
    if (err == OMPI_ERR_BAD_PARAM)
	fprintf(error_out, "ERROR: Initialization of bitmap failed \n\n");

    err = ompi_bitmap_init(&bm, BSIZE);
    if (0 > err) {
	fprintf(error_out, "Error in bitmap create -- aborting \n");
	exit(-1);
    }

    fprintf(error_out, "\nTesting bitmap set... \n");
    test_bitmap_set(&bm);

    fprintf(error_out, "\nTesting bitmap clear ... \n");
    test_bitmap_clear(&bm);

    fprintf(error_out, "\nTesting bitmap is_set ... \n");
    test_bitmap_is_set(&bm);

    fprintf(error_out, "\nTesting bitmap clear_all... \n");
    test_bitmap_clear_all(&bm);

    fprintf(error_out, "\nTesting bitmap set_all... \n");
    test_bitmap_set_all(&bm);

    fprintf(error_out, "\nTesting bitmap find_and_set... \n");
    test_bitmap_find_and_set(&bm);

    fprintf(error_out, "\nTesting bitmap find_size... \n");
    test_bitmap_find_size(&bm);

    fprintf(error_out, "\n~~~~~~     Testing complete     ~~~~~~ \n\n");

    test_finalize();
#ifndef STANDALONE
    fclose(error_out);
#endif

    return 0;
}



void test_bitmap_set(ompi_bitmap_t *bm) {
    int result=0;

    /* start of bitmap and boundaries */
    set_bit(bm, 0);
    set_bit(bm, 1);
    set_bit(bm, 7);
    set_bit(bm, 8);
    /* middle of bitmap  */
    set_bit(bm, 24);

    /* end of bitmap initial size */
    set_bit(bm, 31);
    set_bit(bm, 32);
    
    /* beyond bitmap -- this is valid */
    set_bit(bm, 44);
    set_bit(bm, 82);

    /* invalid bit */
    PRINT_VALID_ERR;
    result = set_bit(bm, -1);
    TEST_AND_REPORT(result, ERR_CODE,"ompi_bitmap_set_bit");
}


void test_bitmap_clear(ompi_bitmap_t *bm) {
    int result=0;

    /* Valid set bits  */
    clear_bit(bm, 29);
    clear_bit(bm, 31);
    clear_bit(bm, 33);
    clear_bit(bm, 32);
    clear_bit(bm, 0);
    
    /* invalid bit */
    PRINT_VALID_ERR;
    result = clear_bit(bm, -1);
    TEST_AND_REPORT(result, ERR_CODE,"ompi_bitmap_clear_bit");
    PRINT_VALID_ERR;
    result = clear_bit(bm, 142);
    TEST_AND_REPORT(result, ERR_CODE,"ompi_bitmap_clear_bit");
}


void test_bitmap_is_set(ompi_bitmap_t *bm)
{
    int result=0;

    /* First set some bits */
    test_bitmap_set(bm);
    is_set_bit(bm, 0);
    is_set_bit(bm, 1);
    is_set_bit(bm, 31);
    is_set_bit(bm, 32);

    PRINT_VALID_ERR;
    result = is_set_bit(bm, 1122);
    TEST_AND_REPORT(result,ERR_CODE,"ompi_bitmap_is_set_bit");    
    PRINT_VALID_ERR;
    is_set_bit(bm, -33);
    TEST_AND_REPORT(result,ERR_CODE,"ompi_bitmap_is_set_bit");    
    PRINT_VALID_ERR;
    is_set_bit(bm, -1);
    TEST_AND_REPORT(result,ERR_CODE,"ompi_bitmap_is_set_bit");    
}


void test_bitmap_find_and_set(ompi_bitmap_t *bm) 
{
    int bsize;
    int result=0;

    ompi_bitmap_clear_all_bits(bm);
    result = find_and_set(bm, 0);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
    result = find_and_set(bm, 1);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
    result = find_and_set(bm, 2);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
    result = find_and_set(bm, 3);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    

    result = ompi_bitmap_set_bit(bm, 5);
    result = find_and_set(bm, 4);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
    
    result = ompi_bitmap_set_bit(bm, 6);
    result = ompi_bitmap_set_bit(bm, 7);

    /* Setting beyond a char boundary */
    result = find_and_set(bm, 8);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
    ompi_bitmap_set_bit(bm, 9);
    result = find_and_set(bm, 10);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    

    /* Setting beyond the current size of bitmap  */
    ompi_bitmap_set_all_bits(bm);
    bsize = bm->array_size * SIZE_OF_CHAR;
    result = find_and_set(bm, bsize);
    TEST_AND_REPORT(result, 0, "ompi_bitmap_find_and_set_first_unset_bit");    
}

void test_bitmap_clear_all(ompi_bitmap_t *bm)
{
    int result = clear_all(bm);
    TEST_AND_REPORT(result, 0, " error in ompi_bitmap_clear_all_bits");
}


void test_bitmap_set_all(ompi_bitmap_t *bm)
{
    int result = set_all(bm);
    TEST_AND_REPORT(result, 0, " error in ompi_bitmap_set_ala_bitsl");
}

void test_bitmap_find_size(ompi_bitmap_t *bm)
{
    int result = find_size(bm);
    TEST_AND_REPORT(result, 0, " error in ompi_bitmap_size");
}


int set_bit(ompi_bitmap_t *bm, int bit)
{
    int err = ompi_bitmap_set_bit(bm, bit);
    if (err != 0 
	|| !(bm->bitmap[bit/SIZE_OF_CHAR] & (1 << bit % SIZE_OF_CHAR))) {
	    fprintf(error_out, "ERROR: set_bit for bit = %d\n\n", bit);
	    return ERR_CODE;
	}
    return 0;
}


int clear_bit(ompi_bitmap_t *bm, int bit)
{
    int err = ompi_bitmap_clear_bit(bm, bit);
    if ((err != 0)
	|| (bm->bitmap[bit/SIZE_OF_CHAR] & (1 << bit % SIZE_OF_CHAR))) {
	fprintf(error_out, "ERROR: clear_bit for bit = %d \n\n", bit);
	return ERR_CODE;
    }

    return 0;
}


int is_set_bit(ompi_bitmap_t *bm, int bit) 
{
    int result = ompi_bitmap_is_set_bit(bm, bit);
    if (((1 == result) 
	&& !(bm->bitmap[bit/SIZE_OF_CHAR] & (1 << bit % SIZE_OF_CHAR)))
	|| (result < 0)
	|| ((0 == result) 
	    &&(bm->bitmap[bit/SIZE_OF_CHAR] & (1 << bit % SIZE_OF_CHAR)))) {
	fprintf(error_out, "ERROR: is_set_bit for bit = %d \n\n",bit);
	return ERR_CODE;
    }
	
    return 0;
}


int find_and_set(ompi_bitmap_t *bm, int bit) 
{
    int ret, pos;
    /* bit here is the bit that should be found and set, in the top
       level stub, this function will be called in sequence to test */

    ret = ompi_bitmap_find_and_set_first_unset_bit(bm, &pos);
    if (ret != OMPI_SUCCESS) return ret;

    if (pos != bit) {
	fprintf(error_out, "ERROR: find_and_set: expected to find_and_set %d\n\n",
		bit);
	return ERR_CODE;
    }

    return 0;
}


int clear_all(ompi_bitmap_t *bm) 
{
    int i, err;
    err = ompi_bitmap_clear_all_bits(bm);
    for (i = 0; i < bm->array_size; ++i)
	if (bm->bitmap[i] != 0) {
	    fprintf(error_out, "ERROR: clear_all for bitmap arry entry %d\n\n",
		    i);
	    return ERR_CODE;
	}
    return 0;
}
	    

int set_all(ompi_bitmap_t *bm)
{
   int i, err;
   err = ompi_bitmap_set_all_bits(bm);
   for (i = 0; i < bm->array_size; ++i)
       if (bm->bitmap[i] != 0xff) {
	   fprintf(error_out, "ERROR: set_all for bitmap arry entry %d\n\n", i);
	   return ERR_CODE;
       }
   return 0;
}

	
int find_size(ompi_bitmap_t *bm)
{
    if (ompi_bitmap_size(bm) != bm->legal_numbits) {
	fprintf(error_out, "ERROR: find_size: expected %d reported %d\n\n",
		(int) bm->array_size, (int) ompi_bitmap_size(bm));
        
	return ERR_CODE;
    }
    return 0;
}


#if WANT_PRINT_BITMAP
void print_bitmap(ompi_bitmap_t *bm) 
{
    /* Accessing the fields within the structure, since its not an
       opaque structure  */

    int i;
    for (i = 0; i < bm->array_size; ++i) {
	fprintf(error_out, "---\n bitmap[%d] = %x \n---\n\n", i, 
		(bm->bitmap[i] & 0xff));
    }
    fprintf(error_out, "========================= \n");
    return;
}
#endif
