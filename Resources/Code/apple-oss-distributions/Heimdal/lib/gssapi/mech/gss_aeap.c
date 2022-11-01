/*
 * AEAD support
 */

#include "mech_locl.h"

/**
 * Encrypts or sign the data.
 *
 * This is a more complicated version of gss_wrap(), it allows the
 * caller to use AEAD data (signed header/trailer) and allow greater
 * controll over where the encrypted data is placed.
 *
 * The maximum packet size is gss_context_stream_sizes.max_msg_size.
 *
 * The caller needs provide the folloing buffers when using in conf_req_flag=1 mode:
 *
 * - HEADER (of size gss_context_stream_sizes.header)
 *   { DATA or SIGN_ONLY } (optional, zero or more)
 *   PADDING (of size gss_context_stream_sizes.blocksize, if zero padding is zero, can be omitted)
 *   TRAILER (of size gss_context_stream_sizes.trailer)
 *
 * - on DCE-RPC mode, the caller can skip PADDING and TRAILER if the
 *   DATA elements is padded to a block bountry and header is of at
 *   least size gss_context_stream_sizes.header + gss_context_stream_sizes.trailer.
 *
 * HEADER, PADDING, TRAILER will be shrunken to the size required to transmit any of them too large.
 *
 * To generate gss_wrap() compatible packets, use: HEADER | DATA | PADDING | TRAILER
 *
 * When used in conf_req_flag=0,
 *
 * - HEADER (of size gss_context_stream_sizes.header)
 *   { DATA or SIGN_ONLY } (optional, zero or more)
 *   PADDING (of size gss_context_stream_sizes.blocksize, if zero padding is zero, can be omitted)
 *   TRAILER (of size gss_context_stream_sizes.trailer)
 *
 *
 * The input sizes of HEADER, PADDING and TRAILER can be fetched using gss_wrap_iov_length() or
 * gss_context_query_attributes().
 *
 * @ingroup gssapi
 */


GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_wrap_iov(OM_uint32 * __nonnull minor_status,
	     __nonnull gss_ctx_id_t  context_handle,
	     int conf_req_flag,
	     gss_qop_t qop_req,
	     int * __nonnull conf_state,
	     gss_iov_buffer_desc *__nonnull iov,
	     int iov_count)
{
	struct _gss_context *ctx = (struct _gss_context *) context_handle;
	gssapi_mech_interface m;

	if (minor_status)
	    *minor_status = 0;
	if (conf_state)
	    *conf_state = 0;
	if (ctx == NULL)
	    return GSS_S_NO_CONTEXT;
	if (iov == NULL && iov_count != 0)
	    return GSS_S_CALL_INACCESSIBLE_READ;

	m = ctx->gc_mech;

	if (m->gm_wrap_iov == NULL)
	    return GSS_S_UNAVAILABLE;

	return (m->gm_wrap_iov)(minor_status, ctx->gc_ctx,
				conf_req_flag, qop_req, conf_state,
				iov, iov_count);
}

/**
 * Decrypt or verifies the signature on the data.
 *
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_unwrap_iov(OM_uint32 * __nonnull minor_status,
	       __nonnull gss_ctx_id_t context_handle,
	       int * __nullable conf_state,
	       gss_qop_t *__nullable qop_state,
	       gss_iov_buffer_desc *__nonnull iov,
	       int iov_count)
{
	struct _gss_context *ctx = (struct _gss_context *) context_handle;
	gssapi_mech_interface m;

	if (minor_status)
	    *minor_status = 0;
	if (conf_state)
	    *conf_state = 0;
	if (qop_state)
	    *qop_state = 0;
	if (ctx == NULL)
	    return GSS_S_NO_CONTEXT;
	if (iov == NULL && iov_count != 0)
	    return GSS_S_CALL_INACCESSIBLE_READ;

	m = ctx->gc_mech;

	if (m->gm_unwrap_iov == NULL)
	    return GSS_S_UNAVAILABLE;

	return (m->gm_unwrap_iov)(minor_status, ctx->gc_ctx,
				  conf_state, qop_state,
				  iov, iov_count);
}

/**
 * Update the length fields in iov buffer for the types:
 * - GSS_IOV_BUFFER_TYPE_HEADER
 * - GSS_IOV_BUFFER_TYPE_PADDING
 * - GSS_IOV_BUFFER_TYPE_TRAILER
 *
 * Consider using gss_context_query_attributes() to fetch the data instead.
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_wrap_iov_length(OM_uint32 * __nonnull minor_status,
		    __nonnull gss_ctx_id_t context_handle,
		    int conf_req_flag,
		    gss_qop_t qop_req,
		    int * __nullable conf_state,
		    gss_iov_buffer_desc *__nonnull iov,
		    int iov_count)
{
	struct _gss_context *ctx = (struct _gss_context *) context_handle;
	gssapi_mech_interface m;

	if (minor_status)
	    *minor_status = 0;
	if (conf_state)
	    *conf_state = 0;
	if (ctx == NULL)
	    return GSS_S_NO_CONTEXT;
	if (iov == NULL && iov_count != 0)
	    return GSS_S_CALL_INACCESSIBLE_READ;

	m = ctx->gc_mech;

	if (m->gm_wrap_iov_length == NULL)
	    return GSS_S_UNAVAILABLE;

	return (m->gm_wrap_iov_length)(minor_status, ctx->gc_ctx,
				       conf_req_flag, qop_req, conf_state,
				       iov, iov_count);
}

/**
 * Free all buffer allocated by gss_wrap_iov() or gss_unwrap_iov() by
 * looking at the GSS_IOV_BUFFER_FLAG_ALLOCATED flag.
 *
 * @ingroup gssapi
 */

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_release_iov_buffer(OM_uint32 * __nonnull minor_status,
		       gss_iov_buffer_desc *__nonnull iov,
		       int iov_count)
{
    OM_uint32 junk;
    int i;

    if (minor_status)
	*minor_status = 0;
    if (iov == NULL && iov_count != 0)
	return GSS_S_CALL_INACCESSIBLE_READ;

    for (i = 0; i < iov_count; i++) {
	if ((iov[i].type & GSS_IOV_BUFFER_FLAG_ALLOCATED) == 0)
	    continue;
	gss_release_buffer(&junk, &iov[i].buffer);
	iov[i].type &= ~GSS_IOV_BUFFER_FLAG_ALLOCATED;
    }
    return GSS_S_COMPLETE;
}

gss_iov_buffer_desc *__nullable
_gss_mg_find_buffer(gss_iov_buffer_desc *__nonnull iov,
		    int iov_count,
		    OM_uint32 type)
{
    int i;

    for (i = 0; i < iov_count; i++)
        if (GSS_IOV_BUFFER_TYPE(iov[i].type) == type)
            return &iov[i];

    return NULL;
}

OM_uint32
_gss_mg_allocate_buffer(OM_uint32 * __nonnull minor_status,
			gss_iov_buffer_desc * __nonnull buffer,
			size_t size)
{
    if (buffer->type & GSS_IOV_BUFFER_TYPE_FLAG_ALLOCATED) {
	if (buffer->buffer.length == size)
	    return GSS_S_COMPLETE;
	free(buffer->buffer.value);
    }

    buffer->buffer.value = malloc(size);
    buffer->buffer.length = size;
    if (buffer->buffer.value == NULL) {
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }
    buffer->type |= GSS_IOV_BUFFER_TYPE_FLAG_ALLOCATED;

    return GSS_S_COMPLETE;
}


/**
 * Query the context for parameters.
 *
 * SSPI equivalent if this function is QueryContextAttributes.
 *
 * - GSS_C_ATTR_STREAM_SIZES data is a gss_context_stream_sizes.
 *
 * @ingroup gssapi
 */

gss_OID_desc GSSAPI_LIB_FUNCTION __gss_c_attr_stream_sizes_oid_desc =
    {10, rk_UNCONST("\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x03")};

GSSAPI_LIB_FUNCTION OM_uint32 GSSAPI_LIB_CALL
gss_context_query_attributes(OM_uint32 * __nonnull minor_status,
			     __nonnull const gss_ctx_id_t context_handle,
			     __nonnull const gss_OID attribute,
			     void *__nonnull data,
			     size_t len)
{
    if (minor_status)
	*minor_status = 0;

    if (gss_oid_equal(GSS_C_ATTR_STREAM_SIZES, attribute)) {
	memset(data, 0, len);
	return GSS_S_COMPLETE;
    }

    return GSS_S_FAILURE;
}

#undef GSS_C_ATTR_STREAM_SIZES
GSSAPI_LIB_VARIABLE gss_OID GSS_C_ATTR_STREAM_SIZES =
    &__gss_c_attr_stream_sizes_oid_desc;

