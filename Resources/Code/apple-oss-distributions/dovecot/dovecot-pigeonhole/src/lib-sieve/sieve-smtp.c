/* Copyright (c) 2002-2013 Pigeonhole authors, see the included COPYING file
 */
#include "lib.h"

#include "sieve-common.h"
#include "sieve-smtp.h"


bool sieve_smtp_available
(const struct sieve_script_env *senv)
{
    return ( senv->smtp_open != NULL && senv->smtp_close != NULL );
}

void *sieve_smtp_open
(const struct sieve_script_env *senv, const char *destination,
    const char *return_path, struct ostream **output_r)
{
    if ( senv->smtp_open == NULL || senv->smtp_close == NULL )
        return NULL;

    return senv->smtp_open(senv, destination, return_path, output_r);
}

bool sieve_smtp_close
(const struct sieve_script_env *senv, void *handle)
{
    if ( senv->smtp_open == NULL || senv->smtp_close == NULL )
        return NULL;

    return senv->smtp_close(senv, handle);
}

