/* Copyright (c) 2002-2013 Pigeonhole authors, see the included COPYING file
 */

/* Extension include
 * -----------------
 *
 * Authors: Stephan Bosch
 * Specification: draft-ietf-sieve-include-01
 * Implementation: mostly full, but required ManageSieve behavior is not
 *                 implemented
 * Status: testing
 *
 */

/* FIXME: Current include implementation does not allow for parts of the script
 * to be located in external binaries; all included scripts are recompiled and
 * the resulting byte code is imported into the main binary in separate blocks.
 */

#include "lib.h"

#include "sieve-common.h"

#include "sieve-extensions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-binary.h"
#include "sieve-dump.h"

#include "sieve-ext-variables.h"

#include "ext-include-common.h"
#include "ext-include-binary.h"
#include "ext-include-variables.h"

/*
 * Operations
 */

static const struct sieve_operation_def *ext_include_operations[] = {
	&include_operation,
	&return_operation,
	&global_operation
};

/*
 * Extension
 */

/* Forward declaration */

static bool ext_include_validator_load
	(const struct sieve_extension *ext, struct sieve_validator *validator);
static bool ext_include_generator_load
	(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv);
static bool ext_include_interpreter_load
	(const struct sieve_extension *ext, const struct sieve_runtime_env *renv,
		sieve_size_t *address);
static bool ext_include_binary_load
	(const struct sieve_extension *ext, struct sieve_binary *binary);

/* Extension objects */

const struct sieve_extension_def include_extension = {
	.name = "include",
	.version = 1,

	.load = ext_include_load,
	.unload = ext_include_unload,
	.validator_load = ext_include_validator_load,
	.generator_load = ext_include_generator_load,
	.interpreter_load = ext_include_interpreter_load,
	.binary_load = ext_include_binary_load,
	.binary_dump = ext_include_binary_dump,
	.code_dump = ext_include_code_dump,

	SIEVE_EXT_DEFINE_OPERATIONS(ext_include_operations)
};

static bool ext_include_validator_load
(const struct sieve_extension *ext, struct sieve_validator *valdtr)
{
	/* Register new commands */
	sieve_validator_register_command(valdtr, ext, &cmd_include);
	sieve_validator_register_command(valdtr, ext, &cmd_return);
	sieve_validator_register_command(valdtr, ext, &cmd_global);

	/* DEPRICATED */
	sieve_validator_register_command(valdtr, ext, &cmd_import);
	sieve_validator_register_command(valdtr, ext, &cmd_export);

	/* Initialize global variables namespace */
	ext_include_variables_global_namespace_init(ext, valdtr);

	return TRUE;
}

static bool ext_include_generator_load
(const struct sieve_extension *ext, const struct sieve_codegen_env *cgenv)
{
	ext_include_register_generator_context(ext, cgenv);

	return TRUE;
}

static bool ext_include_interpreter_load
(const struct sieve_extension *ext, const struct sieve_runtime_env *renv,
	sieve_size_t *address ATTR_UNUSED)
{
	ext_include_interpreter_context_init(ext, renv->interp);

	return TRUE;
}

static bool ext_include_binary_load
(const struct sieve_extension *ext, struct sieve_binary *sbin)
{
	(void)ext_include_binary_get_context(ext, sbin);

	return TRUE;
}
