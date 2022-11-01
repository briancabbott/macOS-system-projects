/* Copyright (c) 2002-2013 Pigeonhole authors, see the included COPYING file
 */

#include "lib.h"
#include "str.h"
#include "strfuncs.h"
#include "md5.h"
#include "hostpid.h"
#include "str-sanitize.h"
#include "ostream.h"
#include "message-address.h"
#include "message-date.h"
#include "ioloop.h"

#include "rfc2822.h"

#include "sieve-common.h"
#include "sieve-stringlist.h"
#include "sieve-code.h"
#include "sieve-address.h"
#include "sieve-extensions.h"
#include "sieve-commands.h"
#include "sieve-actions.h"
#include "sieve-validator.h"
#include "sieve-generator.h"
#include "sieve-interpreter.h"
#include "sieve-dump.h"
#include "sieve-result.h"
#include "sieve-message.h"
#include "sieve-smtp.h"

#include "ext-vacation-common.h"

#include <stdio.h>

/*
 * Forward declarations
 */

static const struct sieve_argument_def vacation_days_tag;
static const struct sieve_argument_def vacation_subject_tag;
static const struct sieve_argument_def vacation_from_tag;
static const struct sieve_argument_def vacation_addresses_tag;
static const struct sieve_argument_def vacation_mime_tag;
static const struct sieve_argument_def vacation_handle_tag;

/*
 * Vacation command
 *
 * Syntax:
 *    vacation [":days" number] [":subject" string]
 *                 [":from" string] [":addresses" string-list]
 *                 [":mime"] [":handle" string] <reason: string>
 */

static bool cmd_vacation_registered
	(struct sieve_validator *valdtr, const struct sieve_extension *ext,
		struct sieve_command_registration *cmd_reg);
static bool cmd_vacation_pre_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_vacation_validate
	(struct sieve_validator *valdtr, struct sieve_command *cmd);
static bool cmd_vacation_generate
	(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd);

const struct sieve_command_def vacation_command = {
	"vacation",
	SCT_COMMAND,
	1, 0, FALSE, FALSE,
	cmd_vacation_registered,
	cmd_vacation_pre_validate,
	cmd_vacation_validate,
	NULL,
	cmd_vacation_generate,
	NULL
};

/*
 * Vacation command tags
 */

/* Forward declarations */

static bool cmd_vacation_validate_number_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);
static bool cmd_vacation_validate_string_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);
static bool cmd_vacation_validate_stringlist_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);
static bool cmd_vacation_validate_mime_tag
	(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
		struct sieve_command *cmd);

/* Argument objects */

static const struct sieve_argument_def vacation_days_tag = {
	"days",
	NULL,
	cmd_vacation_validate_number_tag,
	NULL, NULL, NULL,
};

static const struct sieve_argument_def vacation_seconds_tag = {
	"seconds",
	NULL,
	cmd_vacation_validate_number_tag,
	NULL, NULL, NULL,
};

static const struct sieve_argument_def vacation_subject_tag = {
	"subject",
	NULL,
	cmd_vacation_validate_string_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def vacation_from_tag = {
	"from",
	NULL,
	cmd_vacation_validate_string_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def vacation_addresses_tag = {
	"addresses",
	NULL,
	cmd_vacation_validate_stringlist_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def vacation_mime_tag = {
	"mime",
	NULL,
	cmd_vacation_validate_mime_tag,
	NULL, NULL, NULL
};

static const struct sieve_argument_def vacation_handle_tag = {
	"handle",
	NULL,
	cmd_vacation_validate_string_tag,
	NULL, NULL, NULL
};

/* Codes for optional arguments */

enum cmd_vacation_optional {
	OPT_END,
	OPT_SECONDS,
	OPT_SUBJECT,
	OPT_FROM,
	OPT_ADDRESSES,
	OPT_MIME
};

/*
 * Vacation operation
 */

static bool ext_vacation_operation_dump
	(const struct sieve_dumptime_env *denv, sieve_size_t *address);
static int ext_vacation_operation_execute
	(const struct sieve_runtime_env *renv, sieve_size_t *address);

const struct sieve_operation_def vacation_operation = {
	"VACATION",
	&vacation_extension,
	0,
	ext_vacation_operation_dump,
	ext_vacation_operation_execute
};

/*
 * Vacation action
 */

/* Forward declarations */

static int act_vacation_check_duplicate
	(const struct sieve_runtime_env *renv,
		const struct sieve_action *act,
		const struct sieve_action *act_other);
int act_vacation_check_conflict
	(const struct sieve_runtime_env *renv,
		const struct sieve_action *act,
		const struct sieve_action *act_other);
static void act_vacation_print
	(const struct sieve_action *action,
		const struct sieve_result_print_env *rpenv, bool *keep);
static int act_vacation_commit
	(const struct sieve_action *action,	const struct sieve_action_exec_env *aenv,
		void *tr_context, bool *keep);

/* Action object */

const struct sieve_action_def act_vacation = {
	"vacation",
	SIEVE_ACTFLAG_SENDS_RESPONSE,
	NULL,
	act_vacation_check_duplicate,
	act_vacation_check_conflict,
	act_vacation_print,
	NULL, NULL,
	act_vacation_commit,
	NULL
};

/* Action context information */

struct act_vacation_context {
	const char *reason;

	sieve_number_t seconds;
	const char *subject;
	const char *handle;
	bool mime;
	const char *from;
	const char *from_normalized;
	const char *const *addresses;
};

/*
 * Command validation context
 */

struct cmd_vacation_context_data {
	string_t *from;
	string_t *subject;

	bool mime;

	struct sieve_ast_argument *handle_arg;
};

/*
 * Tag validation
 */

static bool cmd_vacation_validate_number_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
	struct sieve_command *cmd)
{
	const struct sieve_extension *ext = sieve_argument_ext(*arg);
	const struct ext_vacation_config *config =
		(const struct ext_vacation_config *) ext->context;
	struct sieve_ast_argument *tag = *arg;
	sieve_number_t period, seconds;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax:
	 *   :days number
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_NUMBER, FALSE) ) {
		return FALSE;
	}

	period = sieve_ast_argument_number(*arg);
	if ( sieve_argument_is(tag, vacation_days_tag) ) {
		seconds = period * (24*60*60);

	} else if ( sieve_argument_is(tag, vacation_seconds_tag) ) {
		seconds = period;

	} else {
		i_unreached();
	}

	/* Enforce :seconds >= min_period */
	if ( seconds < config->min_period ) {
		seconds = config->min_period;

		sieve_argument_validate_warning(valdtr, *arg,
			"specified :%s value '%lu' is under the minimum",
			sieve_argument_identifier(tag), (unsigned long) period);

	/* Enforce :days <= max_period */
	} else if ( config->max_period > 0 && seconds > config->max_period ) {
		seconds = config->max_period;

		sieve_argument_validate_warning(valdtr, *arg,
			"specified :%s value '%lu' is over the maximum",
			sieve_argument_identifier(tag), (unsigned long) period);
	}

	sieve_ast_argument_number_set(*arg, seconds);

	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

static bool cmd_vacation_validate_string_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;
	struct cmd_vacation_context_data *ctx_data =
		(struct cmd_vacation_context_data *) cmd->data;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax:
	 *   :subject string
	 *   :from string
	 *   :handle string
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING, FALSE) ) {
		return FALSE;
	}

	if ( sieve_argument_is(tag, vacation_from_tag) ) {
		if ( sieve_argument_is_string_literal(*arg) ) {
			string_t *address = sieve_ast_argument_str(*arg);
			const char *error;
	 		bool result;

	 		T_BEGIN {
	 			result = sieve_address_validate(address, &error);

				if ( !result ) {
					sieve_argument_validate_error(valdtr, *arg,
						"specified :from address '%s' is invalid for vacation action: %s",
						str_sanitize(str_c(address), 128), error);
				}
			} T_END;

			if ( !result )
				return FALSE;
		}

		ctx_data->from = sieve_ast_argument_str(*arg);

		/* Skip parameter */
		*arg = sieve_ast_argument_next(*arg);

	} else if ( sieve_argument_is(tag, vacation_subject_tag) ) {
		ctx_data->subject = sieve_ast_argument_str(*arg);

		/* Skip parameter */
		*arg = sieve_ast_argument_next(*arg);

	} else if ( sieve_argument_is(tag, vacation_handle_tag) ) {
		ctx_data->handle_arg = *arg;

		/* Detach optional argument (emitted as mandatory) */
		*arg = sieve_ast_arguments_detach(*arg, 1);
	}

	return TRUE;
}

static bool cmd_vacation_validate_stringlist_tag
(struct sieve_validator *valdtr, struct sieve_ast_argument **arg,
	struct sieve_command *cmd)
{
	struct sieve_ast_argument *tag = *arg;

	/* Detach the tag itself */
	*arg = sieve_ast_arguments_detach(*arg,1);

	/* Check syntax:
	 *   :addresses string-list
	 */
	if ( !sieve_validate_tag_parameter
		(valdtr, cmd, tag, *arg, NULL, 0, SAAT_STRING_LIST, FALSE) ) {
		return FALSE;
	}

	/* Skip parameter */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

static bool cmd_vacation_validate_mime_tag
(struct sieve_validator *valdtr ATTR_UNUSED, struct sieve_ast_argument **arg,
	struct sieve_command *cmd)
{
	struct cmd_vacation_context_data *ctx_data =
		(struct cmd_vacation_context_data *) cmd->data;

	ctx_data->mime = TRUE;

	/* Skip tag */
	*arg = sieve_ast_argument_next(*arg);

	return TRUE;
}

/*
 * Command registration
 */

static bool cmd_vacation_registered
(struct sieve_validator *valdtr, const struct sieve_extension *ext,
	struct sieve_command_registration *cmd_reg)
{
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_days_tag, OPT_SECONDS);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_subject_tag, OPT_SUBJECT);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_from_tag, OPT_FROM);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_addresses_tag, OPT_ADDRESSES);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_mime_tag, OPT_MIME);
	sieve_validator_register_tag
		(valdtr, cmd_reg, ext, &vacation_handle_tag, 0);

	return TRUE;
}

bool ext_vacation_register_seconds_tag
(struct sieve_validator *valdtr, const struct sieve_extension *vacation_ext)
{
	sieve_validator_register_external_tag
		(valdtr, vacation_command.identifier, vacation_ext, &vacation_seconds_tag,
			OPT_SECONDS);

	return TRUE;
}

/*
 * Command validation
 */

static bool cmd_vacation_pre_validate
(struct sieve_validator *valdtr ATTR_UNUSED,
	struct sieve_command *cmd)
{
	struct cmd_vacation_context_data *ctx_data;

	/* Assign context */
	ctx_data = p_new(sieve_command_pool(cmd),
		struct cmd_vacation_context_data, 1);
	cmd->data = ctx_data;

	return TRUE;
}

static const char _handle_empty_subject[] = "<default-subject>";
static const char _handle_empty_from[] = "<default-from>";
static const char _handle_mime_enabled[] = "<MIME>";
static const char _handle_mime_disabled[] = "<NO-MIME>";

static bool cmd_vacation_validate
(struct sieve_validator *valdtr, struct sieve_command *cmd)
{
	struct sieve_ast_argument *arg = cmd->first_positional;
	struct cmd_vacation_context_data *ctx_data =
		(struct cmd_vacation_context_data *) cmd->data;

	if ( !sieve_validate_positional_argument
		(valdtr, cmd, arg, "reason", 1, SAAT_STRING) ) {
		return FALSE;
	}

	if ( !sieve_validator_argument_activate(valdtr, cmd, arg, FALSE) )
		return FALSE;

	/* Construct handle if not set explicitly */
	if ( ctx_data->handle_arg == NULL ) {
		T_BEGIN {
			string_t *handle;
			string_t *reason = sieve_ast_argument_str(arg);
			unsigned int size = str_len(reason);

			/* Precalculate the size of it all */
			size += ctx_data->subject == NULL ?
				sizeof(_handle_empty_subject) - 1 : str_len(ctx_data->subject);
			size += ctx_data->from == NULL ?
				sizeof(_handle_empty_from) - 1 : str_len(ctx_data->from);
			size += ctx_data->mime ?
				sizeof(_handle_mime_enabled) - 1 : sizeof(_handle_mime_disabled) - 1;

			/* Construct the string */
			handle = t_str_new(size);
			str_append_str(handle, reason);

			if ( ctx_data->subject != NULL )
				str_append_str(handle, ctx_data->subject);
			else
				str_append(handle, _handle_empty_subject);

			if ( ctx_data->from != NULL )
				str_append_str(handle, ctx_data->from);
			else
				str_append(handle, _handle_empty_from);

			str_append(handle,
				ctx_data->mime ? _handle_mime_enabled : _handle_mime_disabled );

			/* Create positional handle argument */
			ctx_data->handle_arg = sieve_ast_argument_string_create
				(cmd->ast_node, handle, sieve_ast_node_line(cmd->ast_node));
		} T_END;

		if ( !sieve_validator_argument_activate
			(valdtr, cmd, ctx_data->handle_arg, TRUE) )
			return FALSE;
	} else {
		/* Attach explicit handle argument as positional */
		(void)sieve_ast_argument_attach(cmd->ast_node, ctx_data->handle_arg);
	}

	return TRUE;
}

/*
 * Code generation
 */

static bool cmd_vacation_generate
(const struct sieve_codegen_env *cgenv, struct sieve_command *cmd)
{
	sieve_operation_emit(cgenv->sblock, cmd->ext, &vacation_operation);

	/* Generate arguments */
	if ( !sieve_generate_arguments(cgenv, cmd, NULL) )
		return FALSE;

	return TRUE;
}

/*
 * Code dump
 */

static bool ext_vacation_operation_dump
(const struct sieve_dumptime_env *denv, sieve_size_t *address)
{
	int opt_code = 0;

	sieve_code_dumpf(denv, "VACATION");
	sieve_code_descend(denv);

	/* Dump optional operands */

	for (;;) {
		int opt;
		bool opok = TRUE;

		if ( (opt=sieve_opr_optional_dump(denv, address, &opt_code)) < 0 )
			return FALSE;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_SECONDS:
			opok = sieve_opr_number_dump(denv, address, "seconds");
			break;
		case OPT_SUBJECT:
			opok = sieve_opr_string_dump(denv, address, "subject");
			break;
		case OPT_FROM:
			opok = sieve_opr_string_dump(denv, address, "from");
			break;
		case OPT_ADDRESSES:
			opok = sieve_opr_stringlist_dump(denv, address, "addresses");
			break;
		case OPT_MIME:
			sieve_code_dumpf(denv, "mime");
			break;
		default:
			return FALSE;
		}

		if ( !opok ) return FALSE;
	}

	/* Dump reason and handle operands */
	return
		sieve_opr_string_dump(denv, address, "reason") &&
		sieve_opr_string_dump(denv, address, "handle");
}

/*
 * Code execution
 */

static int ext_vacation_operation_execute
(const struct sieve_runtime_env *renv, sieve_size_t *address)
{
	const struct sieve_extension *this_ext = renv->oprtn->ext;
	const struct ext_vacation_config *config =
		(const struct ext_vacation_config *) this_ext->context;
	struct sieve_side_effects_list *slist = NULL;
	struct act_vacation_context *act;
	pool_t pool;
	int opt_code = 0;
	sieve_number_t seconds = config->default_period;
	bool mime = FALSE;
	struct sieve_stringlist *addresses = NULL;
	string_t *reason, *subject = NULL, *from = NULL, *handle = NULL;
	const char *from_normalized = NULL;
	int ret;

	/*
	 * Read code
	 */

	/* Optional operands */

	for (;;) {
		int opt;

		if ( (opt=sieve_opr_optional_read(renv, address, &opt_code)) < 0 )
			return SIEVE_EXEC_BIN_CORRUPT;

		if ( opt == 0 ) break;

		switch ( opt_code ) {
		case OPT_SECONDS:
			ret = sieve_opr_number_read(renv, address, "seconds", &seconds);
			break;
		case OPT_SUBJECT:
			ret = sieve_opr_string_read(renv, address, "subject", &subject);
			break;
		case OPT_FROM:
			ret = sieve_opr_string_read(renv, address, "from", &from);
			break;
		case OPT_ADDRESSES:
			ret = sieve_opr_stringlist_read(renv, address, "addresses", &addresses);
			break;
		case OPT_MIME:
			mime = TRUE;
			ret = SIEVE_EXEC_OK;
			break;
		default:
			sieve_runtime_trace_error(renv, "unknown optional operand");
			ret = SIEVE_EXEC_BIN_CORRUPT;
		}

		if ( ret <= 0 ) return ret;
	}

	/* Fixed operands */

	if ( (ret=sieve_opr_string_read(renv, address, "reason", &reason)) <= 0 ||
		(ret=sieve_opr_string_read(renv, address, "handle", &handle)) <= 0 ) {
		return ret;
	}

	/*
	 * Perform operation
	 */

	/* Trace */

	if ( sieve_runtime_trace_active(renv, SIEVE_TRLVL_ACTIONS) ) {
		sieve_runtime_trace(renv, 0, "vacation action");
		sieve_runtime_trace_descend(renv);
		sieve_runtime_trace(renv, 0, "auto-reply with message `%s'",
			str_sanitize(str_c(reason), 80));
	}

	/* Check and normalize :from address */
	if ( from != NULL ) {
		const char *error;

		from_normalized = sieve_address_normalize(from, &error);

		if ( from_normalized == NULL) {
			sieve_runtime_error(renv, NULL,
				"specified :from address '%s' is invalid for vacation action: %s",
				str_sanitize(str_c(from), 128), error);
   		}
	}

	/* Add vacation action to the result */

	pool = sieve_result_pool(renv->result);
	act = p_new(pool, struct act_vacation_context, 1);
	act->reason = p_strdup(pool, str_c(reason));
	act->handle = p_strdup(pool, str_c(handle));
	act->seconds = seconds;
	act->mime = mime;
	if ( subject != NULL )
		act->subject = p_strdup(pool, str_c(subject));
	if ( from != NULL ) {
		act->from = p_strdup(pool, str_c(from));
		act->from_normalized = p_strdup(pool, from_normalized);
	}

	/* Normalize all addresses */
	if ( addresses != NULL ) {
		ARRAY(const char *) norm_addresses;
		string_t *raw_address;
		int ret;

		sieve_stringlist_reset(addresses);

		p_array_init(&norm_addresses, pool, 4);

		raw_address = NULL;
		while ( (ret=sieve_stringlist_next_item(addresses, &raw_address)) > 0 ) {
			const char *error;
			const char *addr_norm = sieve_address_normalize(raw_address, &error);

			if ( addr_norm != NULL ) {
				addr_norm = p_strdup(pool, addr_norm);

				array_append(&norm_addresses, &addr_norm, 1);
			} else {
				sieve_runtime_error(renv, NULL,
					"specified :addresses item '%s' is invalid: %s for vacation action "
					"(ignored)",
					str_sanitize(str_c(raw_address),128), error);
			}
		}

		if ( ret < 0 ) {
			sieve_runtime_trace_error(renv, "invalid addresses stringlist");
			return SIEVE_EXEC_BIN_CORRUPT;
		}

		(void)array_append_space(&norm_addresses);
		act->addresses = array_idx(&norm_addresses, 0);
	}

	if ( sieve_result_add_action
		(renv, this_ext, &act_vacation, slist, (void *) act, 0, FALSE) < 0 )
		return SIEVE_EXEC_FAILURE;

	return SIEVE_EXEC_OK;
}

/*
 * Action
 */

/* Runtime verification */

static int act_vacation_check_duplicate
(const struct sieve_runtime_env *renv ATTR_UNUSED,
	const struct sieve_action *act,
	const struct sieve_action *act_other)
{
	if ( !act_other->executed ) {
		sieve_runtime_error(renv, act->location,
			"duplicate vacation action not allowed "
			"(previously triggered one was here: %s)", act_other->location);
		return -1;
	}

	/* Not an error if executed in preceeding script */
	return 1;
}

int act_vacation_check_conflict
(const struct sieve_runtime_env *renv,
	const struct sieve_action *act,
	const struct sieve_action *act_other)
{
	if ( (act_other->def->flags & SIEVE_ACTFLAG_SENDS_RESPONSE) > 0 ) {
		if ( !act_other->executed && !act->executed) {
			sieve_runtime_error(renv, act->location,
				"vacation action conflicts with other action: "
				"the %s action (%s) also sends a response back to the sender",
				act_other->def->name, act_other->location);
			return -1;
		} else {
			/* Not an error if executed in preceeding script */
			return 1;
		}
	}

	return 0;
}

/* Result printing */

static void act_vacation_print
(const struct sieve_action *action ATTR_UNUSED,
	const struct sieve_result_print_env *rpenv, bool *keep ATTR_UNUSED)
{
	struct act_vacation_context *ctx =
		(struct act_vacation_context *) action->context;

	sieve_result_action_printf( rpenv, "send vacation message:");
	sieve_result_printf(rpenv, "    => seconds : %d\n", ctx->seconds);
	if ( ctx->subject != NULL )
		sieve_result_printf(rpenv, "    => subject : %s\n", ctx->subject);
	if ( ctx->from != NULL )
		sieve_result_printf(rpenv, "    => from    : %s\n", ctx->from);
	if ( ctx->handle != NULL )
		sieve_result_printf(rpenv, "    => handle  : %s\n", ctx->handle);
	sieve_result_printf(rpenv, "\nSTART MESSAGE\n%s\nEND MESSAGE\n", ctx->reason);
}

/* Result execution */

/* Headers known to be associated with mailing lists
 */
static const char * const _list_headers[] = {
	"list-id",
	"list-owner",
	"list-subscribe",
	"list-post",
	"list-unsubscribe",
	"list-help",
	"list-archive",
	NULL
};

/* Headers that should be searched for the user's own mail address(es)
 */

static const char * const _my_address_headers[] = {
	"to",
	"cc",
	"bcc",
	"resent-to",
	"resent-cc",
	"resent-bcc",
	NULL
};

static inline bool _is_system_address(const char *address)
{
	if ( strncasecmp(address, "MAILER-DAEMON", 13) == 0 )
		return TRUE;

	if ( strncasecmp(address, "LISTSERV", 8) == 0 )
		return TRUE;

	if ( strncasecmp(address, "majordomo", 9) == 0 )
		return TRUE;

	if ( strstr(address, "-request@") != NULL )
		return TRUE;

	if ( strncmp(address, "owner-", 6) == 0 )
		return TRUE;

	return FALSE;
}

static inline bool _contains_my_address
	(const char * const *headers, const char *my_address)
{
	const char *const *hdsp = headers;
	bool result = FALSE;

	while ( *hdsp != NULL && !result ) {
		const struct message_address *addr;

		T_BEGIN {

			addr = message_address_parse
				(pool_datastack_create(), (const unsigned char *) *hdsp,
					strlen(*hdsp), 256, FALSE);

			while ( addr != NULL && !result ) {
				if (addr->domain != NULL) {
					const char *hdr_address;

					i_assert(addr->mailbox != NULL);

					hdr_address = t_strconcat(addr->mailbox, "@", addr->domain, NULL);
					if ( sieve_address_compare(hdr_address, my_address, TRUE) == 0 ) {
						result = TRUE;
						break;
					}
				}

				addr = addr->next;
			}
		} T_END;

		hdsp++;
	}

	return result;
}

static bool _contains_8bit(const char *text)
{
	const unsigned char *p = (const unsigned char *) text;

	for (; *p != '\0'; p++) {
		if ((*p & 0x80) != 0)
			return TRUE;
	}
	return FALSE;
}

static bool act_vacation_send
(const struct sieve_action_exec_env *aenv, struct act_vacation_context *ctx,
	const char *reply_to, const char *reply_from)
{
	const struct sieve_message_data *msgdata = aenv->msgdata;
	const struct sieve_script_env *senv = aenv->scriptenv;
	void *smtp_handle;
	struct ostream *output;
	string_t *msg;
 	const char *outmsgid;
 	const char *const *headers;
	const char *subject;
	int ret;

	/* Check smpt functions just to be sure */

	if ( !sieve_smtp_available(senv) ) {
		sieve_result_global_warning(aenv,
			"vacation action has no means to send mail");
		return TRUE;
	}

	/* Make sure we have a subject for our reply */

	if ( ctx->subject == NULL || *(ctx->subject) == '\0' ) {
		if ( mail_get_headers_utf8
			(msgdata->mail, "subject", &headers) >= 0 && headers[0] != NULL ) {
			subject = t_strconcat("Auto: ", headers[0], NULL);
		}	else {
			subject = "Automated reply";
		}
	}	else {
		subject = ctx->subject;
	}

	subject = str_sanitize(subject, 256);

	/* Open smtp session */

	smtp_handle = sieve_smtp_open(senv, reply_to, NULL, &output);
	outmsgid = sieve_message_get_new_id(aenv->svinst);

	/* Produce a proper reply */

	msg = t_str_new(512);
	rfc2822_header_write(msg, "X-Sieve", SIEVE_IMPLEMENTATION);
	rfc2822_header_write(msg, "Message-ID", outmsgid);
	rfc2822_header_write(msg, "Date", message_date_create(ioloop_time));

	if ( ctx->from != NULL && *(ctx->from) != '\0' )
		rfc2822_header_utf8_printf(msg, "From", "%s", ctx->from);
	else if ( reply_from != NULL )
		rfc2822_header_printf(msg, "From", "<%s>", reply_from);
	else
		rfc2822_header_printf(msg, "From", "Postmaster <%s>", senv->postmaster_address);

	/* FIXME: If From header of message has same address, we should use that
	 * instead to properly include the phrase part.
	 */
	rfc2822_header_printf(msg, "To", "<%s>", reply_to);

	if ( _contains_8bit(subject) )
		rfc2822_header_utf8_printf(msg, "Subject", "%s", subject);
	else
		rfc2822_header_printf(msg, "Subject", "%s", subject);

	/* Compose proper in-reply-to and references headers */

	ret = mail_get_headers
		(aenv->msgdata->mail, "references", &headers);

	if ( msgdata->id != NULL ) {
		rfc2822_header_write(msg, "In-Reply-To", msgdata->id);

		if ( ret >= 0 && headers[0] != NULL )
			rfc2822_header_write
				(msg, "References", t_strconcat(headers[0], " ", msgdata->id, NULL));
		else
			rfc2822_header_write(msg, "References", msgdata->id);
	} else if ( ret >= 0 && headers[0] != NULL ) {
		rfc2822_header_write(msg, "References", headers[0]);
	}

	rfc2822_header_write(msg, "Auto-Submitted", "auto-replied (vacation)");
	rfc2822_header_write(msg, "Precedence", "bulk");

	rfc2822_header_write(msg, "MIME-Version", "1.0");

	if ( !ctx->mime ) {
		rfc2822_header_write(msg, "Content-Type", "text/plain; charset=utf-8");
		rfc2822_header_write(msg, "Content-Transfer-Encoding", "8bit");
		str_append(msg, "\r\n");
	}

	str_printfa(msg, "%s\r\n", ctx->reason);
  o_stream_send(output, str_data(msg), str_len(msg));

	/* Close smtp session */
	if ( !sieve_smtp_close(senv, smtp_handle) ) {
		sieve_result_global_error(aenv,
			"failed to send vacation response to <%s> "
			"(refer to server log for more information)",
			str_sanitize(reply_to, 128));
		return FALSE;
	}

	return TRUE;
}

static void act_vacation_hash
(struct act_vacation_context *vctx, const char *sender, unsigned char hash_r[])
{
	const char *rpath = t_str_lcase(sender);
	struct md5_context ctx;

	md5_init(&ctx);
	md5_update(&ctx, rpath, strlen(rpath));

	md5_update(&ctx, vctx->handle, strlen(vctx->handle));

	md5_final(&ctx, hash_r);
}

static int act_vacation_commit
(const struct sieve_action *action, const struct sieve_action_exec_env *aenv,
	void *tr_context ATTR_UNUSED, bool *keep ATTR_UNUSED)
{
	const struct sieve_extension *ext = action->ext;
	const struct ext_vacation_config *config =
		(const struct ext_vacation_config *) ext->context;
	const struct sieve_message_data *msgdata = aenv->msgdata;
	const struct sieve_script_env *senv = aenv->scriptenv;
	struct act_vacation_context *ctx =
		(struct act_vacation_context *) action->context;
	unsigned char dupl_hash[MD5_RESULTLEN];
	struct mail *mail = sieve_message_get_mail(aenv->msgctx);
	const char *sender = sieve_message_get_sender(aenv->msgctx);
	const char *recipient = sieve_message_get_final_recipient(aenv->msgctx);
	const char *const *hdsp;
	const char *const *headers;
	const char *reply_from = NULL, *orig_recipient = NULL;
	bool result;

	/* Is the recipient unset?
	 */
	if ( recipient == NULL ) {
		sieve_result_global_warning
			(aenv, "vacation action aborted: envelope recipient is <>");
		return SIEVE_EXEC_OK;
	}

	/* Is the return path unset ?
	 */
	if ( sender == NULL ) {
		sieve_result_global_log(aenv, "discarded vacation reply to <>");
		return SIEVE_EXEC_OK;
	}

	/* Are we perhaps trying to respond to ourselves ?
	 */
	if ( sieve_address_compare(sender, recipient, TRUE) == 0 ) {
		sieve_result_global_log(aenv,
			"discarded vacation reply to own address <%s>",
			str_sanitize(sender, 128));
		return SIEVE_EXEC_OK;
	}

	/* Are we perhaps trying to respond to one of our alternative :addresses?
	 */
	if ( ctx->addresses != NULL ) {
		const char * const *alt_address = ctx->addresses;

		while ( *alt_address != NULL ) {
			if ( sieve_address_compare(sender, *alt_address, TRUE) == 0 ) {
				sieve_result_global_log(aenv,
					"discarded vacation reply to own address <%s> "
					"(as specified using :addresses argument)",
					str_sanitize(sender, 128));
				return SIEVE_EXEC_OK;
			}
			alt_address++;
		}
	}

	/* Did whe respond to this user before? */
	if ( sieve_action_duplicate_check_available(senv) ) {
		act_vacation_hash(ctx, sender, dupl_hash);

		if ( sieve_action_duplicate_check(senv, dupl_hash, sizeof(dupl_hash)) )
		{
			sieve_result_global_log(aenv,
				"discarded duplicate vacation response to <%s>",
				str_sanitize(sender, 128));
			return SIEVE_EXEC_OK;
		}
	}

	/* Are we trying to respond to a mailing list ? */
	hdsp = _list_headers;
	while ( *hdsp != NULL ) {
		if ( mail_get_headers
			(mail, *hdsp, &headers) >= 0 && headers[0] != NULL ) {
			/* Yes, bail out */
			sieve_result_global_log(aenv,
				"discarding vacation response to mailinglist recipient <%s>",
				str_sanitize(sender, 128));
			return SIEVE_EXEC_OK;
		}
		hdsp++;
	}

	/* Is the message that we are replying to an automatic reply ? */
	if ( mail_get_headers
		(msgdata->mail, "auto-submitted", &headers) >= 0 ) {
		/* Theoretically multiple headers could exist, so lets make sure */
		hdsp = headers;
		while ( *hdsp != NULL ) {
			if ( strcasecmp(*hdsp, "no") != 0 ) {
				sieve_result_global_log(aenv,
					"discarding vacation response to auto-submitted message from <%s>",
 					str_sanitize(sender, 128));
					return SIEVE_EXEC_OK;
			}
			hdsp++;
		}
	}

	/* Check for the (non-standard) precedence header */
	if ( mail_get_headers
		(mail, "precedence", &headers) >= 0 ) {
		/* Theoretically multiple headers could exist, so lets make sure */
		hdsp = headers;
		while ( *hdsp != NULL ) {
			if ( strcasecmp(*hdsp, "junk") == 0 || strcasecmp(*hdsp, "bulk") == 0 ||
				strcasecmp(*hdsp, "list") == 0 ) {
				sieve_result_global_log(aenv,
					"discarding vacation response to precedence=%s message from <%s>",
					*hdsp, str_sanitize(sender, 128));
					return SIEVE_EXEC_OK;
			}
			hdsp++;
		}
	}

	/* Do not reply to system addresses */
	if ( _is_system_address(sender) ) {
		sieve_result_global_log(aenv,
			"not sending vacation response to system address <%s>",
			str_sanitize(sender, 128));
		return SIEVE_EXEC_OK;
	}

	/* Fetch original recipient if necessary */
	if ( config->use_original_recipient  )
		orig_recipient = sieve_message_get_orig_recipient(aenv->msgctx);

	/* Is the original message directly addressed to the user or the addresses
	 * specified using the :addresses tag?
	 */
	hdsp = _my_address_headers;
	while ( *hdsp != NULL ) {
		if ( mail_get_headers
			(mail, *hdsp, &headers) >= 0 && headers[0] != NULL ) {

			/* Final recipient directly listed in headers? */
			if ( _contains_my_address(headers, recipient) ) {
				reply_from = recipient;
				break;
			}

			/* Original recipient directly listed in headers? */
			if ( orig_recipient != NULL &&
				_contains_my_address(headers, orig_recipient) ) {
				reply_from = orig_recipient;
				break;
			}

			/* User-provided :addresses listed in headers? */
			if ( ctx->addresses != NULL ) {
				bool found = FALSE;
				const char * const *my_address = ctx->addresses;

				while ( !found && *my_address != NULL ) {
					if ( (found=_contains_my_address(headers, *my_address)) )
						reply_from = *my_address;
					my_address++;
				}

				if ( found ) break;
			}
		}
		hdsp++;
	}

	/* My address not found in the headers; we got an implicit delivery */
	if ( *hdsp == NULL ) {
		if ( config->dont_check_recipient ) {
			/* Send reply from envelope recipient address */
			reply_from = recipient;

		} else {
			const char *original_recipient = "";

			/* Bail out */

			if ( config->use_original_recipient ) {
				original_recipient = t_strdup_printf("original-recipient=<%s>, ",
					( orig_recipient == NULL ? "UNAVAILABLE" :
						str_sanitize(orig_recipient, 128) ));
			}

			sieve_result_global_log(aenv,
				"discarding vacation response for implicitly delivered message; "
				"no known (envelope) recipient address found in message headers "
				"(recipient=<%s>, %sand%s additional `:addresses' are specified)",
				str_sanitize(recipient, 128), original_recipient,
				(ctx->addresses == NULL ? " no" : ""));

			return SIEVE_EXEC_OK;
		}
	}

	/* Send the message */

	T_BEGIN {
		result = act_vacation_send(aenv, ctx, sender, reply_from);
	} T_END;

	if ( result ) {
		sieve_number_t seconds;

		sieve_result_global_log(aenv, "sent vacation response to <%s>",
			str_sanitize(sender, 128));

		/* Check period limits once more */
		seconds = ctx->seconds;
		if ( seconds < config->min_period )
			seconds = config->min_period;
		else if ( config->max_period > 0 && seconds > config->max_period )
			seconds = config->max_period;

		/* Mark as replied */
		if ( seconds > 0  ) {
			sieve_action_duplicate_mark
				(senv, dupl_hash, sizeof(dupl_hash), ioloop_time + seconds);
		}
	}

	return SIEVE_EXEC_OK;
}




