/* Copyright (c) 2002-2013 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "array.h"
#include "str-sanitize.h"
#include "home-expand.h"

#include "sieve-common.h"
#include "sieve-settings.h"
#include "sieve-error.h"
#include "sieve-script.h"
#include "sieve-ast.h"
#include "sieve-binary.h"
#include "sieve-commands.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"

#include "ext-include-common.h"
#include "ext-include-limits.h"
#include "ext-include-binary.h"
#include "ext-include-variables.h"

#include <stdlib.h>

/*
 * Forward declarations
 */

/* Generator context */

struct ext_include_generator_context {
	unsigned int nesting_depth;
	struct sieve_script *script;
	struct ext_include_generator_context *parent;
};

static inline struct ext_include_generator_context *
	ext_include_get_generator_context
	(const struct sieve_extension *ext_this, struct sieve_generator *gentr);

/* Interpreter context */

struct ext_include_interpreter_global {
	ARRAY(struct sieve_script *) included_scripts;

	struct sieve_variable_scope_binary *var_scope;
	struct sieve_variable_storage *var_storage;
};

struct ext_include_interpreter_context {
	struct ext_include_interpreter_context *parent;
	struct ext_include_interpreter_global *global;

	struct sieve_interpreter *interp;
	pool_t pool;

	unsigned int nesting_depth;

	struct sieve_script *script;
	const struct ext_include_script_info *script_info;

	const struct ext_include_script_info *include;
	bool returned;
};

/*
 * Extension configuration
 */

/* Extension hooks */

bool ext_include_load
(const struct sieve_extension *ext, void **context)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_include_context *ctx;
	const char *sieve_dir;
	unsigned long long int uint_setting;

	if ( *context != NULL ) {
		ext_include_unload(ext);
	}

	// FIXME: sieve_dir and sieve_global_dir settings have somewhat become
	// misnomers; these are not necessary filesystem directories anymore.

	ctx = i_new(struct ext_include_context, 1);

	/* Get directory for :global scripts */
	sieve_dir = sieve_setting_get(svinst, "sieve_global_dir");

	if ( sieve_dir == NULL && svinst->debug ) {
		sieve_sys_debug(svinst, "include: sieve_global_dir is not set; "
			"it is currently not possible to include `:global' scripts.");
	}

	ctx->global_dir = i_strdup(sieve_dir);

	/* Get directory for :personal scripts */
 	sieve_dir = sieve_setting_get(svinst, "sieve_dir");

	if ( sieve_dir == NULL ) {
		sieve_dir = "~/sieve";
	}

	ctx->personal_dir = i_strdup(sieve_dir);

	/* Get limits */
	ctx->max_nesting_depth = EXT_INCLUDE_DEFAULT_MAX_NESTING_DEPTH;
	ctx->max_includes = EXT_INCLUDE_DEFAULT_MAX_INCLUDES;

	if ( sieve_setting_get_uint_value
		(svinst, "sieve_include_max_nesting_depth", &uint_setting) ) {
		ctx->max_nesting_depth = (unsigned int) uint_setting;
	}

	if ( sieve_setting_get_uint_value
		(svinst, "sieve_include_max_includes", &uint_setting) ) {
		ctx->max_includes = (unsigned int) uint_setting;
	}

	/* Extension dependencies */
	ctx->var_ext = sieve_ext_variables_get_extension(ext->svinst);

	*context = (void *)ctx;

	return TRUE;
}

void ext_include_unload
(const struct sieve_extension *ext)
{
	struct ext_include_context *ctx =
		(struct ext_include_context *) ext->context;

	i_free(ctx->personal_dir);
	i_free(ctx->global_dir);
	i_free(ctx);
}

/*
 * Script access
 */

const char *ext_include_get_script_location
(const struct sieve_extension *ext, enum ext_include_script_location location,
   const char *script_name)
{
	struct sieve_instance *svinst = ext->svinst;
	struct ext_include_context *ctx =
		(struct ext_include_context *) ext->context;
	const char *sieve_dir;

	switch ( location ) {
	case EXT_INCLUDE_LOCATION_PERSONAL:

		if ( ctx->personal_dir == NULL ) {
			sieve_sys_error(svinst, "include: sieve_dir is unconfigured; "
				"include of `:personal' script `%s' is therefore not possible",
				str_sanitize(script_name, 80));
			return NULL;
		}

		sieve_dir = ctx->personal_dir;
		break;

	case EXT_INCLUDE_LOCATION_GLOBAL:

		if ( ctx->global_dir == NULL ) {
			sieve_sys_error(svinst, "include: sieve_global_dir is unconfigured; "
				"include of `:global' script `%s' is therefore not possible",
				str_sanitize(script_name, 80));
			return NULL;
		}

		sieve_dir = ctx->global_dir;
		break;
	default:
		i_unreached();
	}

	return sieve_dir;
}

/*
 * AST context management
 */

static void ext_include_ast_free
(const struct sieve_extension *ext ATTR_UNUSED,
	struct sieve_ast *ast ATTR_UNUSED, void *context)
{
	struct ext_include_ast_context *actx =
		(struct ext_include_ast_context *) context;
	struct sieve_script **scripts;
	unsigned int count, i;

	/* Unreference included scripts */
	scripts = array_get_modifiable(&actx->included_scripts, &count);
	for ( i = 0; i < count; i++ ) {
		sieve_script_unref(&scripts[i]);
	}

	/* Unreference variable scopes */
	if ( actx->global_vars != NULL )
		sieve_variable_scope_unref(&actx->global_vars);
}

static const struct sieve_ast_extension include_ast_extension = {
	&include_extension,
	ext_include_ast_free
};

struct ext_include_ast_context *ext_include_create_ast_context
(const struct sieve_extension *this_ext, struct sieve_ast *ast,
	struct sieve_ast *parent)
{
	struct ext_include_ast_context *actx;

	pool_t pool = sieve_ast_pool(ast);
	actx = p_new(pool, struct ext_include_ast_context, 1);
	p_array_init(&actx->included_scripts, pool, 32);

	if ( parent != NULL ) {
		struct ext_include_ast_context *parent_ctx =
			(struct ext_include_ast_context *)
				sieve_ast_extension_get_context(parent, this_ext);
		actx->global_vars = parent_ctx->global_vars;

		i_assert( actx->global_vars != NULL );

		sieve_variable_scope_ref(actx->global_vars);
	} else {
		actx->global_vars = sieve_variable_scope_create(this_ext->svinst, this_ext);
	}

	sieve_ast_extension_register
		(ast, this_ext, &include_ast_extension, (void *) actx);

	return actx;
}

struct ext_include_ast_context *ext_include_get_ast_context
(const struct sieve_extension *this_ext, struct sieve_ast *ast)
{
	struct ext_include_ast_context *actx = (struct ext_include_ast_context *)
		sieve_ast_extension_get_context(ast, this_ext);

	if ( actx != NULL ) return actx;

	return ext_include_create_ast_context(this_ext, ast, NULL);
}

void ext_include_ast_link_included_script
(const struct sieve_extension *this_ext, struct sieve_ast *ast,
	struct sieve_script *script)
{
	struct ext_include_ast_context *actx =
		ext_include_get_ast_context(this_ext, ast);

	array_append(&actx->included_scripts, &script, 1);
}

bool ext_include_validator_have_variables
(const struct sieve_extension *this_ext, struct sieve_validator *valdtr)
{
	struct ext_include_context *ectx = ext_include_get_context(this_ext);

	return sieve_ext_variables_is_active(ectx->var_ext, valdtr);
}

/*
 * Generator context management
 */

static struct ext_include_generator_context *
ext_include_create_generator_context
(struct sieve_generator *gentr, struct ext_include_generator_context *parent,
	struct sieve_script *script)
{
	struct ext_include_generator_context *ctx;

	pool_t pool = sieve_generator_pool(gentr);
	ctx = p_new(pool, struct ext_include_generator_context, 1);
	ctx->parent = parent;
	ctx->script = script;
	if ( parent == NULL ) {
		ctx->nesting_depth = 0;
	} else {
		ctx->nesting_depth = parent->nesting_depth + 1;
	}

	return ctx;
}

static inline struct ext_include_generator_context *
	ext_include_get_generator_context
(const struct sieve_extension *this_ext, struct sieve_generator *gentr)
{
	return (struct ext_include_generator_context *)
		sieve_generator_extension_get_context(gentr, this_ext);
}

static inline void ext_include_initialize_generator_context
(const struct sieve_extension *this_ext, struct sieve_generator *gentr,
	struct ext_include_generator_context *parent, struct sieve_script *script)
{
	sieve_generator_extension_set_context(gentr, this_ext,
		ext_include_create_generator_context(gentr, parent, script));
}

void ext_include_register_generator_context
(const struct sieve_extension *this_ext, const struct sieve_codegen_env *cgenv)
{
	struct ext_include_generator_context *ctx =
		ext_include_get_generator_context(this_ext, cgenv->gentr);

	/* Initialize generator context if necessary */
	if ( ctx == NULL ) {
		ctx = ext_include_create_generator_context(
			cgenv->gentr, NULL, cgenv->script);

		sieve_generator_extension_set_context
			(cgenv->gentr, this_ext, (void *) ctx);
	}

	/* Initialize ast context if necessary */
	(void)ext_include_get_ast_context(this_ext, cgenv->ast);
	(void)ext_include_binary_init(this_ext, cgenv->sbin, cgenv->ast);
}

/*
 * Runtime initialization
 */

static void ext_include_runtime_init
(const struct sieve_extension *this_ext, const struct sieve_runtime_env *renv,
	void *context)
{
	struct ext_include_interpreter_context *ctx =
		(struct ext_include_interpreter_context *) context;
	struct ext_include_context *ectx = ext_include_get_context(this_ext);


	if ( ctx->parent == NULL ) {
		ctx->global = p_new(ctx->pool, struct ext_include_interpreter_global, 1);
		p_array_init(&ctx->global->included_scripts, ctx->pool, 10);


		ctx->global->var_scope =
			ext_include_binary_get_global_scope(this_ext, renv->sbin);
		ctx->global->var_storage =
			sieve_variable_storage_create(ctx->pool, ctx->global->var_scope);
	} else {
		ctx->global = ctx->parent->global;
	}

	sieve_ext_variables_runtime_set_storage
		(ectx->var_ext, renv, this_ext, ctx->global->var_storage);
}

static struct sieve_interpreter_extension include_interpreter_extension = {
	&include_extension,
	ext_include_runtime_init,
	NULL,
};

/*
 * Interpreter context management
 */

static struct ext_include_interpreter_context *
	ext_include_interpreter_context_create
(struct sieve_interpreter *interp,
	struct ext_include_interpreter_context *parent,
	struct sieve_script *script, const struct ext_include_script_info *sinfo)
{
	struct ext_include_interpreter_context *ctx;

	pool_t pool = sieve_interpreter_pool(interp);
	ctx = p_new(pool, struct ext_include_interpreter_context, 1);
	ctx->pool = pool;
	ctx->parent = parent;
	ctx->interp = interp;
	ctx->script = script;
	ctx->script_info = sinfo;

	if ( parent == NULL ) {
		ctx->nesting_depth = 0;
	} else {
		ctx->nesting_depth = parent->nesting_depth + 1;
	}

	return ctx;
}

static inline struct ext_include_interpreter_context *
	ext_include_get_interpreter_context
(const struct sieve_extension *this_ext, struct sieve_interpreter *interp)
{
	return (struct ext_include_interpreter_context *)
		sieve_interpreter_extension_get_context(interp, this_ext);
}

static inline struct ext_include_interpreter_context *
	ext_include_interpreter_context_init_child
(const struct sieve_extension *this_ext, struct sieve_interpreter *interp,
	struct ext_include_interpreter_context *parent,
	struct sieve_script *script, const struct ext_include_script_info *sinfo)
{
	struct ext_include_interpreter_context *ctx =
		ext_include_interpreter_context_create(interp, parent, script, sinfo);

	sieve_interpreter_extension_register
		(interp, this_ext, &include_interpreter_extension, ctx);

	return ctx;
}

void ext_include_interpreter_context_init
(const struct sieve_extension *this_ext, struct sieve_interpreter *interp)
{
	struct ext_include_interpreter_context *ctx =
		ext_include_get_interpreter_context(this_ext, interp);

	/* Is this is the top-level interpreter ? */
	if ( ctx == NULL ) {
		struct sieve_script *script;

		/* Initialize top context */
		script = sieve_interpreter_script(interp);
		ctx = ext_include_interpreter_context_create
			(interp, NULL, script, NULL);

		sieve_interpreter_extension_register
			(interp, this_ext, &include_interpreter_extension, (void *) ctx);
	}
}

struct sieve_variable_storage *ext_include_interpreter_get_global_variables
(const struct sieve_extension *this_ext, struct sieve_interpreter *interp)
{
	struct ext_include_interpreter_context *ctx =
		ext_include_get_interpreter_context(this_ext, interp);

	return ctx->global->var_storage;
}

/*
 * Including a script during code generation
 */

int ext_include_generate_include
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd,
	enum ext_include_script_location location, 	enum ext_include_flags flags,
	struct sieve_script *script,
	const struct ext_include_script_info **included_r)
{
	const struct sieve_extension *this_ext = cmd->ext;
	struct ext_include_context *ext_ctx =
		(struct ext_include_context *)this_ext->context;
	int result = 1;
	struct sieve_ast *ast;
	struct sieve_binary *sbin = cgenv->sbin;
	struct sieve_generator *gentr = cgenv->gentr;
	struct ext_include_binary_context *binctx;
	struct sieve_generator *subgentr;
	struct ext_include_generator_context *ctx =
		ext_include_get_generator_context(this_ext, gentr);
	struct ext_include_generator_context *pctx;
	struct sieve_error_handler *ehandler = sieve_generator_error_handler(gentr);
	const struct ext_include_script_info *included;

	*included_r = NULL;

	/* Just to be sure: do not include more scripts when errors have occured
	 * already.
	 */
	if ( sieve_get_errors(ehandler) > 0 )
		return -1;

	/* Limit nesting level */
	if ( ctx->nesting_depth >= ext_ctx->max_nesting_depth ) {
		sieve_command_generate_error
			(gentr, cmd, "cannot nest includes deeper than %d levels",
				ext_ctx->max_nesting_depth);
		return -1;
	}

	/* Check for circular include */
	if ( (flags & EXT_INCLUDE_FLAG_ONCE) == 0 ) {
		pctx = ctx;
		while ( pctx != NULL ) {
			if ( sieve_script_equals(pctx->script, script) ) {
				/* Just drop circular include when uploading inactive script;
				 * not an error
				 */
				if ( (cgenv->flags & SIEVE_COMPILE_FLAG_UPLOADED) != 0 &&
					(cgenv->flags & SIEVE_COMPILE_FLAG_ACTIVATED) == 0 ) {
					sieve_command_generate_warning
						(gentr, cmd, "circular include (ignored during upload)");
					return 0;
				}

				sieve_command_generate_error(gentr, cmd, "circular include");
				return -1;
			}

			pctx = pctx->parent;
		}
	}

	/* Get binary context */
	binctx = ext_include_binary_init(this_ext, sbin, cgenv->ast);

	/* Is the script already compiled into the current binary? */
	if ( ext_include_binary_script_is_included(binctx, script, &included) ) {
		/* Yes, only update flags */
		if ( (flags & EXT_INCLUDE_FLAG_OPTIONAL) == 0 )
			flags &= ~EXT_INCLUDE_FLAG_OPTIONAL;
		if ( (flags & EXT_INCLUDE_FLAG_ONCE) == 0 ) // for consistency
			flags &= ~EXT_INCLUDE_FLAG_ONCE;

	} else 	{
		const char *script_name = sieve_script_name(script);
		enum sieve_compile_flags cpflags = cgenv->flags;

		/* No, include new script */

		/* Check whether include limit is exceeded */
		if ( ext_include_binary_script_get_count(binctx) >=
			ext_ctx->max_includes ) {
	 		sieve_command_generate_error(gentr, cmd,
	 			"failed to include script '%s': no more than %u includes allowed",
				str_sanitize(script_name, 80), ext_ctx->max_includes);
	 		return -1;
		}

		/* Allocate a new block in the binary and mark the script as included.
		 */
		if ( !sieve_script_is_open(script) ) {
			/* Just making an empty entry to mark a missing script */
			i_assert((flags & EXT_INCLUDE_FLAG_MISSING_AT_UPLOAD) != 0 ||
				(flags & EXT_INCLUDE_FLAG_OPTIONAL) != 0);
			included = ext_include_binary_script_include
				(binctx, location, flags, script, NULL);
			result = 0;

		}	else {
			struct sieve_binary_block *inc_block = sieve_binary_block_create(sbin);

			/* Real include */
			included = ext_include_binary_script_include
				(binctx, location, flags, script, inc_block);

			/* Parse */
			if ( (ast = sieve_parse(script, ehandler, NULL)) == NULL ) {
		 		sieve_command_generate_error(gentr, cmd,
		 			"failed to parse included script '%s'", str_sanitize(script_name, 80));
		 		return -1;
			}

			/* Included scripts inherit global variable scope */
			(void)ext_include_create_ast_context(this_ext, ast, cmd->ast_node->ast);

			if ( location == EXT_INCLUDE_LOCATION_GLOBAL )
				cpflags &= ~SIEVE_RUNTIME_FLAG_NOGLOBAL;
			else
				cpflags |= SIEVE_RUNTIME_FLAG_NOGLOBAL;

			/* Validate */
			if ( !sieve_validate(ast, ehandler, cpflags, NULL) ) {
				sieve_command_generate_error(gentr, cmd,
					"failed to validate included script '%s'",
					str_sanitize(script_name, 80));
		 		sieve_ast_unref(&ast);
		 		return -1;
		 	}

			/* Generate
			 *
			 * FIXME: It might not be a good idea to recurse code generation for
			 * included scripts.
			 */
		 	subgentr = sieve_generator_create(ast, ehandler, cpflags);
			ext_include_initialize_generator_context(cmd->ext, subgentr, ctx, script);
	
			if ( sieve_generator_run(subgentr, &inc_block) == NULL ) {
				sieve_command_generate_error(gentr, cmd,
					"failed to generate code for included script '%s'",
					str_sanitize(script_name, 80));
		 		result = -1;
			}

			sieve_generator_free(&subgentr);

			/* Cleanup */
			sieve_ast_unref(&ast);
		}
	}
	
	if ( result > 0 ) *included_r = included;

	return result;
}

/*
 * Executing an included script during interpretation
 */

static int ext_include_runtime_check_circular
(struct ext_include_interpreter_context *ctx,
	const struct ext_include_script_info *include)
{
	struct ext_include_interpreter_context *pctx;

	pctx = ctx;
	while ( pctx != NULL ) {

		if ( sieve_script_equals(include->script, pctx->script) )
			return TRUE;

		pctx = pctx->parent;
	}

	return FALSE;
}

static bool ext_include_runtime_include_mark
(struct ext_include_interpreter_context *ctx,
	const struct ext_include_script_info *include, bool once)
{
	struct sieve_script *const *includes;
	unsigned int count, i;

	includes = array_get(&ctx->global->included_scripts, &count);
	for ( i = 0; i < count; i++ )	{

		if ( sieve_script_equals(include->script, includes[i]) )
			return ( !once );
	}

	array_append(&ctx->global->included_scripts, &include->script, 1);

	return TRUE;
}

int ext_include_execute_include
(const struct sieve_runtime_env *renv, unsigned int include_id, bool once)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	int result = SIEVE_EXEC_OK;
	struct ext_include_interpreter_context *ctx;
	const struct ext_include_script_info *included;
	struct ext_include_binary_context *binctx =
		ext_include_binary_get_context(this_ext, renv->sbin);
	unsigned int block_id;

	/* Check for invalid include id (== corrupt binary) */
	included = ext_include_binary_script_get_included(binctx, include_id);
	if ( included == NULL ) {
		sieve_runtime_trace_error(renv, "include: include id %d is invalid",
			include_id);
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	ctx = ext_include_get_interpreter_context(this_ext, renv->interp);

	block_id = sieve_binary_block_get_id(included->block);

	/* If :once modifier is specified, check for duplicate include */
	if ( ext_include_runtime_include_mark(ctx, included, once) ) {
		sieve_runtime_trace(renv, SIEVE_TRLVL_NONE,
			"include: start script '%s' [inc id: %d, block: %d]",
			sieve_script_name(included->script), include_id, block_id);
	} else {
		/* skip */
		sieve_runtime_trace(renv, SIEVE_TRLVL_NONE,
			"include: skipped include for script '%s' [inc id: %d, block: %d]; "
			"already run once",
			sieve_script_name(included->script), include_id, block_id);
		return result;
	}

	/* Check circular include during interpretation as well.
	 * Let's not trust binaries.
	 */
	if ( ext_include_runtime_check_circular(ctx, included) ) {
		sieve_runtime_trace_error(renv,
			"include: circular include of script '%s' [inc id: %d, block: %d]",
			sieve_script_name(included->script), include_id, block_id);

		/* Situation has no valid way to emerge at runtime */
		return SIEVE_EXEC_BIN_CORRUPT;
	}

	if ( ctx->parent == NULL ) {
		struct ext_include_interpreter_context *curctx = NULL;
		struct sieve_error_handler *ehandler = renv->ehandler;
		struct sieve_interpreter *subinterp;
		bool interrupted = FALSE;

		/* We are the top-level interpreter instance */

		if ( result == SIEVE_EXEC_OK ) {
			enum sieve_runtime_flags rtflags = 0;

			if ( included->location != EXT_INCLUDE_LOCATION_GLOBAL )
				rtflags |= SIEVE_RUNTIME_FLAG_NOGLOBAL;

			/* Create interpreter for top-level included script
			 * (first sub-interpreter)
			 */
			subinterp = sieve_interpreter_create_for_block
				(included->block, included->script, renv->msgdata, renv->scriptenv,
					ehandler, rtflags);

			if ( subinterp != NULL ) {
				curctx = ext_include_interpreter_context_init_child
					(this_ext, subinterp, ctx, included->script, included);

				/* Activate and start the top-level included script */
				result = ( sieve_interpreter_start
					(subinterp, renv->result, &interrupted) == 1 );
			} else
				result = SIEVE_EXEC_BIN_CORRUPT;
		}

		/* Included scripts can have includes of their own. This is not implemented
		 * recursively. Rather, the sub-interpreter interrupts and defers the
		 * include to the top-level interpreter, which is here.
		 */
		if ( result == SIEVE_EXEC_OK && interrupted && !curctx->returned ) {
			while ( result == SIEVE_EXEC_OK ) {

				if ( ( (interrupted && curctx->returned) || (!interrupted) ) &&
					curctx->parent != NULL ) {
					const struct ext_include_script_info *ended_script =
						curctx->script_info;

					/* Sub-interpreter ended or executed return */

					/* Ascend interpreter stack */
					curctx = curctx->parent;
					sieve_interpreter_free(&subinterp);

					sieve_runtime_trace(renv, SIEVE_TRLVL_NONE,
						"include: script '%s' ended [inc id: %d, block: %d]",
						sieve_script_name(ended_script->script), ended_script->id,
						sieve_binary_block_get_id(ended_script->block));

					/* This is the top-most sub-interpreter, bail out */
					if ( curctx->parent == NULL ) break;

					subinterp = curctx->interp;

					/* Continue parent */
					curctx->include = NULL;
					curctx->returned = FALSE;

					result = ( sieve_interpreter_continue(subinterp, &interrupted) == 1 );
				} else {
					if ( curctx->include != NULL ) {
						/* Sub-include requested */

						if ( result == SIEVE_EXEC_OK ) {
							enum sieve_runtime_flags rtflags = 0;

							if ( curctx->include->location != EXT_INCLUDE_LOCATION_GLOBAL )
								rtflags |= SIEVE_RUNTIME_FLAG_NOGLOBAL;

							/* Create sub-interpreter */
							subinterp = sieve_interpreter_create_for_block
								(curctx->include->block, curctx->include->script, renv->msgdata,
									renv->scriptenv, ehandler, rtflags);

							if ( subinterp != NULL ) {
								curctx = ext_include_interpreter_context_init_child
									(this_ext, subinterp, curctx, curctx->include->script,
										curctx->include);

								/* Start the sub-include's interpreter */
								curctx->include = NULL;
								curctx->returned = FALSE;
								result = ( sieve_interpreter_start(subinterp, renv->result,
									&interrupted) == 1 );
							} else
								result = SIEVE_EXEC_BIN_CORRUPT;
						}
					} else {
						/* Sub-interpreter was interrupted outside this extension, probably
						 * stop command was executed. Generate an interrupt ourselves,
						 * ending all script execution.
						 */
						sieve_interpreter_interrupt(renv->interp);
						break;
					}
				}
			}
		}

		/* Free any sub-interpreters that might still be active */
		while ( curctx != NULL && curctx->parent != NULL ) {
			struct ext_include_interpreter_context *nextctx	= curctx->parent;
			struct sieve_interpreter *killed_interp = curctx->interp;
			const struct ext_include_script_info *ended_script =
				curctx->script_info;

			/* This kills curctx too */
			sieve_interpreter_free(&killed_interp);

			sieve_runtime_trace(renv, SIEVE_TRLVL_NONE,
				"include: script '%s' ended [id: %d, block: %d]",
				sieve_script_name(ended_script->script),
				ended_script->id, sieve_binary_block_get_id(ended_script->block));

			/* Luckily we recorded the parent earlier */
			curctx = nextctx;
		}

	} else {
		/* We are an included script already, defer inclusion to main interpreter */

		ctx->include = included;
		sieve_interpreter_interrupt(renv->interp);
	}

	return result;
}

void ext_include_execute_return
(const struct sieve_runtime_env *renv)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	struct ext_include_interpreter_context *ctx =
		ext_include_get_interpreter_context(this_ext, renv->interp);

	sieve_runtime_trace(renv, SIEVE_TRLVL_COMMANDS,
		"return: exiting included script");

	ctx->returned = TRUE;
	sieve_interpreter_interrupt(renv->interp);
}

