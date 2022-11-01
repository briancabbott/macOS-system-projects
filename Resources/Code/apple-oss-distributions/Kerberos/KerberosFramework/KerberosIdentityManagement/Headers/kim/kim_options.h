/*
 * Copyright 2005-2006 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef KIM_OPTIONS_H
#define KIM_OPTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kim/kim_types.h>

/*!
 * \addtogroup kim_types_reference
 * @{
 */

/*!
 * Specifies the user's default options.
 */
#define KIM_OPTIONS_DEFAULT           ((kim_options_t) NULL)

/*!
 * Specifies that credentials should be valid immediately.
 */
#define KIM_OPTIONS_START_IMMEDIATELY ((kim_time_t) 0)

/*!
 * The type of prompt which needs to be displayed.
 * This value determines what type of user interface is displayed.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
typedef uint32_t kim_prompt_type_t;

enum kim_prompt_type_enum {
    kim_prompt_type_password = 0,
    kim_prompt_type_challenge = 1
};

/*!
 * The prompt callback used to display a prompt to the user.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
typedef kim_error_code_t (*kim_prompt_callback_t) (kim_options_t       *io_options, 
                                                   kim_prompt_type_t    in_type,
                                                   kim_string_t         in_title,
                                                   kim_string_t         in_message,
                                                   kim_string_t         in_description,
                                                   void               **out_reply);

/*!
 * The default prompt callback.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
const kim_prompt_callback_t kim_prompt_callback_default;

/*!
 * The graphical prompt callback.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
const kim_prompt_callback_t kim_prompt_callback_gui;

/*!
 * The command line prompt callback.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
const kim_prompt_callback_t kim_prompt_callback_cli;

/*!
 * The prompt callback which always returns an error.
 * Use to turn off prompting entirely.
 * \note Using this callback may prevent the user from authenicating.
 * See \ref kim_options_custom_prompt_callback for more information.
 */
const kim_prompt_callback_t kim_prompt_callback_none;

/*! @} */

/*!
 * \page kim_options_overview KIM Options Overview
 *
 * \section kim_options_introduction Introduction
 *
 * Kerberos Identity Management Options (kim_options_t) allows you to control how
 * the Kerberos library obtains credentials.  When the options structure is initialized with
 * #kim_options_create(), each option is filled in with a default value which can then be modified
 * with the kim_options_set_*() APIs.  If you only want to use the default values, you may pass 
 * #KIM_OPTIONS_DEFAULT into any KIM function that takes a kim_options_t.
 * 
 * KIM options fall into two major categories: options for controlling how credentials are 
 * acquired and options for controlling what properties the newly acquired credentials will have:
 *
 * \section kim_options_credential_acquisition Options for Controlling Credential Acquisition
 * 
 * In order to acquire credentials, Kerberos needs to obtain one or more secrets from the user.
 * These secrets may be a certificate, password, SecurID pin, or information from a smart card.  
 * If obtaining the secret requires interaction with the user, the Kerberos libraries call a
 * "prompter callback" to display a dialog or command line prompt to request information from
 * the user.  If you want to provide your own custom dialogs or command line prompts, 
 * the KIM APIs provide a mechanism for replacing the default prompt callbacks with your own.  
 *
 * \subsection kim_options_custom_prompt_callback Providing a Custom Prompt Callback
 *
 * All secrets are obtained from the user through a #kim_prompt_callback_t.  By default, 
 * options use #kim_prompt_callback_default, which presents an expanding dialog to request
 * information from the user, or if no graphical access is available, a command line prompt.
 * 
 * KIM also provides three other callbacks: #kim_prompt_callback_gui only presents
 * a dialog and returns an error if there is no graphical access. #kim_prompt_callback_cli
 * only presents a command line interface and returns an error if there is no controlling
 * terminal available.  #kim_prompt_callback_none always returns an error.
 *
 * Using #kim_options_set_prompt_callback(), you can change the prompt callback to one of 
 * the above callbacks or a callback you have defined yourself.  Callbacks are called in a
 * loop, one for each prompt.  Because network traffic may occur between calls to the prompt
 * callback, your prompt interface should support time passing between calls to the prompter.  
 * If you are defining a callback yourself, you should also set your own options data with 
 * #kim_options_set_data() for storing state between calls.  Options data is a caller
 * defined pointer value -- the Kerberos libaries make no use of it.
 *
 * \subsection kim_options_preset_prompts Prefetching Prompt Responses
 * 
 * Sometimes you may have already collected some of the information needed to acquire 
 * Kerberos credentials.  Rather than creating a prompt callback, you may also prefetch 
 * responses to the options directly with #kim_options_set_prompt_response().  Once you 
 * have associated your response with a given prompt type, the Kerberos libraries will 
 * use this response for the first prompt of that type rather than calling the prompt 
 * callback to obtain it.
 *
 * Note that even if you prefetch responses, the prompt callback may still be called if
 * you did not provide all the information required for the identity.  You may specify 
 * the #kim_prompt_callback_none prompt callback to prevent prompting from occuring entirely,
 * however, doing so will tie your application to a particular Kerberos configuration.  
 * For example, if your application assumes that all identities only require a password, 
 * it will not be able to acquire credentials at sites using SecurID pins.  
 *
 * 
 * \section kim_options_credential_properties Options for Controlling Credential Properties
 *
 * Kerberos credentials have a number of different properties which can be requested
 * when credentials are acquired.  These properties control when and for how long the 
 * credentials are valid and what you can do with them.  
 
 * Note that setting these properties in the KIM options only changes what the Kerberos 
 * libraries \em request from the KDC.  The KDC itself may choose not to honor your 
 * requested properties if they violate the site security policy.  For example, most sites 
 * place an upper bound on how long credentials may be valid.  If you request a credential 
 * lifetime longer than this upper bound, the KDC may return credentials with a shorter 
 * lifetime than you requested.
 *
 * \subsection kim_options_lifetimes Credential Lifetime
 *
 * Kerberos credentials have start time and a lifetime during which they are valid.  
 * Once the lifetime has passed, credentials "expire" and can no longer be used.  
 *
 * The requested credential start time can be set with #kim_options_set_start_time() 
 * and examined with #kim_options_get_start_time().  The requested credential
 * lifetime can be set with #kim_options_set_lifetime() and examined with
 * #kim_options_get_lifetime().
 * 
 * \subsection kim_options_renewable Renewable Credentials
 *
 * Credentials with very long lifetimes are more convenient since the user does not
 * have authenticate as often.  Unfortunately they are also a higher security 
 * risk: if credentials are stolen they can be used until they expire.
 * Credential renewal exists to compromise between these two conflicting goals.
 *
 * Renewable credentials are TGT credentials which can be used to obtain new
 * TGT credentials without reauthenticating.  By regularly renewing credentials
 * the KDC has an opportunity to check to see if the client's credentials have been
 * reported stolen and refuse to renew them.  Renewable credentials have a "renewal
 * lifetime" during which credentials can be renewed.  This lifetime is relative
 * to the original credential start time.  If credentials are renewed shortly before
 * the end of the renewal lifetime, their lifetime will be capped to the end of the
 * renewal lifetime.
 *
 * Note that credentials must be valid to be renewed and therefore may not be 
 * an appropriate solution for all use cases.  Sites which use renewable
 * credentials often create helper processes running as the user which will 
 * automatically renew the user's credentials when they get close to expiration.
 * 
 * Use #kim_options_set_renewable() to change whether or not the Kerberos libraries
 * request renewable credentials and #kim_options_get_renewable() to find out the 
 * current setting.  Use #kim_options_set_renewal_lifetime() to change the requested
 * renewal lifetime and #kim_options_get_renewal_lifetime() to find out the current 
 * value.
 *
 * \subsection kim_options_addressless Addressless Credentials
 *
 * Traditionally Kerberos used the host's IP address as a mechanism to restrict 
 * the user's credentials to a specific host, thus making it harder to use stolen 
 * credentials.  When authenticating to a remote service with credentials containing
 * addresses, the remote service verifies that the client's IP address is one of the 
 * addresses listed in the credential.  Unfortunately, modern network technologies 
 * such as NAT rewrite the IP address in transit, making it difficult to use 
 * credentials with addresses in them.  As a result, most Kerberos sites now obtain 
 * addressless credentials. 
 *
 * Use #kim_options_set_addressless() to change whether or not the Kerberos libraries
 * request addressless credentials.  Use #kim_options_get_addressless() to find out the 
 * current setting.
 *
 * \subsection kim_options_forwardable Forwardable Credentials
 *
 * Forwardable credentials are TGT credentials which can be forwarded to a service 
 * you have authenticated to.  If the credentials contain IP addresses, the addresses 
 * are changed to reflect the service's IP address.  Credential forwarding is most 
 * commonly used for Kerberos-authenticated remote login services.  By forwarding 
 * TGT credentials through the remote login service, the user's credentials will 
 * appear on the remote host when the user logs in.  
 *
 * The forwardable flag only applies to TGT credentials.
 *
 * Use #kim_options_set_forwardable() to change whether or not the Kerberos libraries
 * request forwardable credentials.  Use #kim_options_get_forwardable() to find out the 
 * current setting.
 *
 * \subsection kim_options_proxiable Proxiable Credentials
 *
 * Proxiable credentials are similar to forwardable credentials except that instead of
 * forwarding the a TGT credential itself, a service credential is forwarded
 * instead.  Using proxiable credentials, a user can permit a service to perform
 * a specific task as the user using one of the user's service credentials.  
 *
 * Like forwardability, the proxiable flag only applies to TGT credentials.  Unlike
 * forwarded credentials, the IP address of proxiable credentials are not modified for  
 * the service when being proxied.  This can be solved by also requesting addressless
 * credentials.
 *
 * Use #kim_options_set_proxiable() to change whether or not the Kerberos libraries
 * request proxiable credentials.  Use #kim_options_get_proxiable() to find out the 
 * current setting.
 *
 * \subsection kim_options_service_identity Service Identity
 *
 * Normally users acquire TGT credentials (ie "ticket granting tickets") and then 
 * use those credentials to acquire service credentials.  This allows Kerberos to 
 * provide single sign-on while still providing mutual authentication to services.  
 * However, sometimes you just want an initial credential for a service.  KIM 
 * options allows you to set the service identity with 
 * #kim_options_set_service_identity() and query it with 
 * #kim_options_get_service_identity().
 *
 * See \ref kim_options_reference for information on specific APIs.
 */ 

/*!
 * \defgroup kim_options_reference KIM Options Reference Documentation
 * @{
 */

/*!
 * \param out_options on exit, a new options object.  Must be freed with kim_options_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Create new options with default values.
 */
kim_error_t kim_options_create (kim_options_t *out_options);

/*!
 * \param out_options on exit, a new options object which is a copy of \a in_options.  
 *                    Must be freed with kim_options_free().
 * \param in_options  a options object. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Copy options.
 */
kim_error_t kim_options_copy (kim_options_t *out_options,
                              kim_options_t  in_options);

/*!
 * \param io_options         an options object to modify.
 * \param in_prompt_callback a prompt callback function.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the prompt callback for obtaining information from the user.
 * \par Default value
 * #kim_prompt_callback_default
 * \sa kim_options_get_prompt_callback()
 */
kim_error_t kim_options_set_prompt_callback (kim_options_t         io_options,
                                             kim_prompt_callback_t in_prompt_callback);

/*!
 * \param in_options          an options object.
 * \param out_prompt_callback on exit, the prompt callback specified by in_options. 
 *                            Does not need to be freed but may become invalid when 
 *                            \a in_options is freed.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the prompt callback for obtaining information from the user.
 * \par Default value
 * #kim_prompt_callback_default
 * \sa kim_options_set_prompt_callback()
 */
kim_error_t kim_options_get_prompt_callback (kim_options_t          in_options,
                                             kim_prompt_callback_t *out_prompt_callback);

/*!
 * \param io_options  an options object to modify.
 * \param in_data     a pointer to caller-specific data.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set caller-specific data for use in library callbacks.
 * \note This option can be used by the caller to store a pointer to data needed when handling a 
 *       callback.  The KIM library does not use this options data in any way.
 * \par Default value
 * NULL (no data is set by default)
 * \sa kim_options_get_data()
 */
kim_error_t kim_options_set_data (kim_options_t  io_options,
                                  void          *in_data);

/*!
 * \param in_options  an options object.
 * \param out_data    on exit, the pointer to caller specific data specified by in_options.
 *                    Does not need to be freed but may become invalid when \a in_options is freed.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get caller-specific data for use in library callbacks.
 * \note This option can be used by the caller to store a pointer to data needed when handling a 
 *       callback.  The KIM library does not use this options data in any way.
 * \par Default value
 * NULL (no data is set by default)
 * \sa kim_options_set_data()
 */
kim_error_t kim_options_get_data (kim_options_t   in_options,
                                  void          **out_data);

/*!
 * \param io_options     an options object to modify.
 * \param in_prompt_type a type of prompt.
 * \param in_response    a response to prompts of \a in_prompt_type.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set a response for a prompt for use when acquiring credentials.
 * \note Each response only overrides the first prompt of a given prompt type.  If multiple
 * prompts of the same type are required, or if a prompt of a different type is requested, 
 * the prompt callback will be called to obtain user input.  If you want to turn off prompting 
 * entirely, call #kim_options_set_prompt_callback() with #kim_prompt_callback_none.
 * \par Default value
 * NULL (no response is set by default)
 * \sa kim_options_get_prompt_response()
 */
kim_error_t kim_options_set_prompt_response (kim_options_t     io_options,
                                             kim_prompt_type_t in_prompt_type,
                                             void             *in_response);

/*!
 * \param in_options     an options object.
 * \param in_prompt_type a type of prompt.
 * \param out_response   on exit, the response to prompts of type \a in_prompt_type specified 
 *                       by \a in_options.  Does not need to be freed but may become invalid
 *                       when \a in_options is freed.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the response for a prompt for use when acquiring credentials.
 * \note Each response only overrides the first prompt of a given prompt type.  If multiple
 * prompts of the same type are required, or if a prompt of a different type is requested, 
 * the prompt callback will be called to obtain user input.  If you want to turn off prompting 
 * entirely, call #kim_options_set_prompt_callback() with #kim_prompt_callback_none.
 * \par Default value
 * NULL (no response is set by default)
 * \sa kim_options_set_prompt_response()
 */
kim_error_t kim_options_get_prompt_response (kim_options_t       in_options,
                                             kim_prompt_type_t   in_prompt_type,
                                             void              **out_response);

/*!
 * \param io_options    an options object to modify.
 * \param in_start_time a start date (in seconds since January 1, 1970).  Set to  
 *                      #KIM_OPTIONS_START_IMMEDIATELY for the acquired credential to be valid 
 *                      immediately.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the date when a credential should become valid.
 * \note When using a start time in the future, once the start time has been reached the credential 
 *       must be validated before it can be used. 
 * \par Default value
 * 0, indicating "now".  The credential will be valid immediately.
 * \sa kim_options_get_start_time(), kim_credential_validate(), kim_ccache_validate(), kim_identity_validate()
 */
kim_error_t kim_options_set_start_time (kim_options_t io_options,
                                        kim_time_t    in_start_time);

/*!
 * \param in_options     an options object.
 * \param out_start_time on exit, the start date (in seconds since January 1, 1970) specified by 
 *                       \a in_options. #KIM_OPTIONS_START_IMMEDIATELY indicates the credential
 *                       will be valid immediately.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the date when a credential should become valid.
 * \note When using a start time in the future, once the start time has been reached the credential 
 *       must be validated before it can be used.
 * \par Default value
 * 0, indicating "now".  The credential will be valid immediately.
 * \sa kim_options_set_start_time(), kim_credential_validate(), kim_ccache_validate(), kim_identity_validate()
 */
kim_error_t kim_options_get_start_time (kim_options_t  in_options,
                                        kim_time_t    *out_start_time);

/*!
 * \param io_options  an options object to modify.
 * \param in_lifetime a lifetime duration (in seconds).
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the duration during which a credential should be valid.
 * \note KDCs have a maximum allowed lifetime per identity (usually 10 to 21 hours).
 *       As a result the credential will actually have a lifetime which is the minimum of
 *       \a in_lifetime and the KDC's maximum allowed lifetime.
 * \sa kim_options_get_lifetime()
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. 10 hours if unspecified.
 */
kim_error_t kim_options_set_lifetime (kim_options_t  io_options,
                                      kim_lifetime_t in_lifetime);

/*!
 * \param in_options   an options object.
 * \param out_lifetime on exit, the lifetime duration (in seconds) specified in \a in_options.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the duration during which an acquired credential should be valid.
 * \note KDCs have a maximum allowed lifetime per identity (usually 10 to 21 hours).
 *       As a result the credential will actually have a lifetime which is the minimum of
 *       \a in_lifetime and the KDC's maximum allowed lifetime.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. 10 hours if unspecified.
 * \sa kim_options_set_lifetime()
 */
kim_error_t kim_options_get_lifetime (kim_options_t   in_options,
                                      kim_lifetime_t *out_lifetime);

/*!
* \param io_options    an options object to modify.
 * \param in_renewable a boolean value indicating whether or not to request a renewable 
 *                     credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to request a renewable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_get_renewable()
 */
kim_error_t kim_options_set_renewable (kim_options_t io_options,
                                       kim_boolean_t in_renewable);

/*!
* \param in_options     an options object.
 * \param out_renewable on exit, a boolean value indicating whether or \a in_options will 
 *                      request a renewable credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to request a renewable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_set_renewable()
 */
kim_error_t kim_options_get_renewable (kim_options_t  in_options,
                                       kim_boolean_t *out_renewable);

/*!
 * \param io_options          an options object to modify.
 * \param in_renewal_lifetime a renewal lifetime duration (in seconds).
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the duration during which a valid credential should be renewable.
 * \note KDCs have a maximum allowed renewal lifetime per identity (usually 10 to 21 hours).
 *       As a result the credential will actually have a lifetime which is the minimum of
 *       \a in_lifetime and the KDC's maximum allowed lifetime.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. 7 days if unspecified.
 * \sa kim_options_get_renewal_lifetime(), kim_identity_renew(), kim_credential_renew(), kim_ccache_renew()
 */
kim_error_t kim_options_set_renewal_lifetime (kim_options_t  io_options,
                                              kim_lifetime_t in_renewal_lifetime);

/*!
 * \param in_options   an options object.
 * \param out_renewal_lifetime on exit, the renewal lifetime duration (in seconds) specified  
 *                             in \a in_options.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the duration during which a valid credential should be renewable.
 * \note KDCs have a maximum allowed lifetime per identity (usually 10 to 21 hours).
 *       As a result the credential will actually have a lifetime which is the minimum of
 *       \a in_lifetime and the KDC's maximum allowed lifetime.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. 7 days if unspecified.
 * \sa kim_options_set_renewal_lifetime(), kim_identity_renew(), kim_credential_renew(), kim_ccache_renew()
 */
kim_error_t kim_options_get_renewal_lifetime (kim_options_t   in_options,
                                              kim_lifetime_t *out_renewal_lifetime);

/*!
 * \param io_options     an options object to modify.
 * \param in_forwardable a boolean value indicating whether or not to request a forwardable 
 *                       credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to request a forwardable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_get_forwardable()
 */
kim_error_t kim_options_set_forwardable (kim_options_t io_options,
                                         kim_boolean_t in_forwardable);

/*!
 * \param in_options      an options object.
 * \param out_forwardable on exit, a boolean value indicating whether or \a in_options will 
 *                        request a forwardable credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to request a forwardable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_set_forwardable()
 */
kim_error_t kim_options_get_forwardable (kim_options_t  in_options,
                                         kim_boolean_t *out_forwardable);

/*!
 * \param io_options   an options object to modify.
 * \param in_proxiable a boolean value indicating whether or not to request a proxiable 
 *                     credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to request a proxiable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_get_proxiable()
 */
kim_error_t kim_options_set_proxiable (kim_options_t io_options,
                                       kim_boolean_t in_proxiable);

/*!
 * \param in_options    an options object.
 * \param out_proxiable on exit, a boolean value indicating whether or \a in_options will 
 *                      request a proxiable credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to request a proxiable credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_set_proxiable()
 */
kim_error_t kim_options_get_proxiable (kim_options_t  in_options,
                                       kim_boolean_t *out_proxiable);

/*!
 * \param io_options     an options object to modify.
 * \param in_addressless a boolean value indicating whether or not to request an addressless 
 *                       credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to request an addressless credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_get_addressless()
 */
kim_error_t kim_options_set_addressless (kim_options_t io_options,
                                         kim_boolean_t in_addressless);

/*!
 * \param in_options      an options object.
 * \param out_addressless on exit, a boolean value indicating whether or \a in_options will 
 *                        request an addressless credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to request an addressless credential.
 * \par Default value
 * Read from the user's preferences and the Kerberos configuration. TRUE if unspecified.
 * \sa kim_options_set_addressless()
 */
kim_error_t kim_options_get_addressless (kim_options_t  in_options,
                                         kim_boolean_t *out_addressless);

/*!
 * \param io_options           an options object to modify.
 * \param in_service_identity  a service identity.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the service identity to request a credential for.
 * \par Default value
 * NULL, indicating "krbtgt@<REALM>", the ticket granting ticket (TGT) service.
 * \sa kim_options_get_service_identity()
 */
kim_error_t kim_options_set_service_identity (kim_options_t  io_options,
                                              kim_identity_t in_service_identity);

/*!
 * \param in_options            an options object.
 * \param out_service_identity  on exit, the service identity specified in \a in_options.
 *                              Must be freed with kim_identity_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the service identity to request a credential for.
 * \par Default value
 * NULL, indicating "krbtgt@<REALM>", the ticket granting ticket (TGT) service.
 * \sa kim_options_set_service_identity()
 */
kim_error_t kim_options_get_service_identity (kim_options_t   in_options,
                                              kim_identity_t *out_service_identity);

/*!
 * \param io_options the options object to be freed.  Set to NULL on exit.
 * \brief Free memory associated with an options object.
 */
void kim_options_free (kim_options_t *io_options);

/*!@}*/

#ifdef __cplusplus
}
#endif

#endif /* KIM_OPTIONS_H */
