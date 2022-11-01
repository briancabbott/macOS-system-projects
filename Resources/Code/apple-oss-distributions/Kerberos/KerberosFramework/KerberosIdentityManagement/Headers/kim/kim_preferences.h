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

#ifndef KIM_PREFERENCES_H
#define KIM_PREFERENCES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kim/kim_types.h>
    
/*!
 * \page kim_favorite_identities_overview KIM Favorite Identities Overview
 *
 * \section kim_favorite_identities_introduction Introduction
 *
 * As Kerberos becomes more widespread, the number of possible Kerberos
 * identities and realms a user might want to use will become very large.
 * Sites may list hundreds of realms in their Kerberos configuration files. 
 * In addition, sites may wish to use DNS SRV records to avoid having to list
 * all the realms they use in their Kerberos configuration.  As a result, the 
 * list of realms in the Kerberos configuration may be exceedingly large and/or 
 * incomplete.  Users may also use multiple identities from the same realm.
 *
 * On platforms which use a GUI to acquire credentials, the KIM would like
 * to to display a list of identities for the user to select from.  Depending on 
 * what is appropriate for the platform, identities may be displayed in a popup 
 * menu or other list.  
 *
 * To solve this problem, the KIM maintains a list of favorite identities 
 * specifically for identity selection.  This list is a set of unique identities 
 * in alphabetical order (as appropriate for the user's language localization).  
 *
 * On most platforms the list of favorite identities has both an administrator 
 * preference and a user preference which overrides it.  The administrator 
 * preference exists only to initialize the favorite identities for new user 
 * accounts.  Once the user modifies the list their favorite identities may  
 * diverge from the site favorite identities preference.
 *
 * \note The location of user preferences and the semantics of
 * preference synchronization is platform-specific.  Where possible KIM will use
 * platform-specific preference mechanisms.
 *
 * Most callers will not need to use the favorite identities APIs.  However if you
 * are implementing your own graphical prompt callback or a credential management 
 * application, you may to view and/or edit the user's favorite identities.
 *
 * \section kim_favorite_identities_edit Viewing and Editing the Favorite Identities
 * 
 * First, you need to acquire the Favorite Identities stored in the user's
 * preferences using #kim_preferences_create() and 
 * #kim_preferences_get_favorite_identities().  Or you can use 
 * #kim_favorite_identities_create() to get an empty identities list if you want to 
 * overwrite the user's identities list entirely.  See \ref kim_preferences_overview 
 * for more information on modifying the user's preferences.
 * 
 * Then use #kim_favorite_identities_get_number_of_identities() and 
 * #kim_favorite_identities_get_identity_at_index() to display the identities list.  
 * Use #kim_favorite_identities_add_identity() and #kim_favorite_identities_remove_identity() 
 * to change which identities are in the identities list.  Identities are always stored in
 * alphabetical order and duplicate identities are not permitted, so when you add or remove a
 * identity you should redisplay the entire list.
 *
 * Once you are done editing the identities list, store changes in the user's preference file
 * using #kim_preferences_set_favorite_identities() and #kim_preferences_synchronize().
 *
 * See \ref kim_favorite_identities_reference for information on specific APIs.
 */

/*!
 * \defgroup kim_favorite_identities_reference KIM Favorite Identities Documentation
 * @{
 */

/*!
 * \param out_favorite_identities on exit, a new favorite identities object.  
 *                            Must be freed with kim_favorite_identities_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Create a new favorite identities list.
 */
kim_error_t kim_favorite_identities_create (kim_favorite_identities_t *out_favorite_identities);

/*!
 * \param out_favorite_identities on exit, a new favorite identities object which is 
 *                            a copy of in_favorite_identities.  
 *                            Must be freed with kim_favorite_identities_free().
 * \param in_favorite_identities  a favorite identities object. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Copy a favorite identities list.
 */
kim_error_t kim_favorite_identities_copy (kim_favorite_identities_t *out_favorite_identities,
                                          kim_favorite_identities_t  in_favorite_identities);

/*!
 * \param in_favorite_identities   a favorite identities object.
 * \param out_number_of_identities on exit, the number of identities in \a in_favorite_identities.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the number of identities in a favorite identities list.
 */
kim_error_t kim_favorite_identities_get_number_of_identities (kim_favorite_identities_t  in_favorite_identities,
                                                              kim_count_t               *out_number_of_identities);

/*!
 * \param in_favorite_identities a favorite identities object.
 * \param in_index           a index into the identities list (starting at 0).
 * \param out_realm          on exit, the identity at \a in_index in \a in_favorite_identities.
 *                           Must be freed with kim_string_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the Nth identity in a favorite identities list.
 */
kim_error_t kim_favorite_identities_get_identity_at_index (kim_favorite_identities_t  in_favorite_identities,
                                                           kim_count_t                in_index,
                                                           kim_identity_t            *out_identity);
/*!
 * \param io_favorite_identities a favorite identities object.
 * \param in_identity            an identity string to add to \a in_favorite_identities.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Add an identity to a favorite identities list.
 */
kim_error_t kim_favorite_identities_add_identity (kim_favorite_identities_t io_favorite_identities,
                                                  kim_identity_t            in_identity);

/*!
 * \param io_favorite_identities a favorite identities object.
 * \param in_identity            an identity to remove from \a in_favorite_identities.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Remove an identity from a identities list.
 */
kim_error_t kim_favorite_identities_remove_identity (kim_favorite_identities_t io_favorite_identities,
                                                     kim_identity_t            in_identity);

/*!
 * \param io_favorite_identities a favorite identities object.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Empty a favorite identities list.
 */
kim_error_t kim_favorite_identities_remove_all_identities (kim_favorite_identities_t io_favorite_identities);

/*!
 * \param io_favorite_identities the favorite identities object to be freed.  Set to NULL on exit.
 * \brief Free memory associated with an identities list.
 */

void kim_favorite_identities_free (kim_favorite_identities_t *io_favorite_identities);

/*!@}*/

/*!
 * \page kim_preferences_overview KIM Preferences Overview
 *
 * \section kim_preferences_introduction Introduction
 *
 * In addition to the site preferences stored in the Kerberos configuration, users may also
 * want to have their own personal preferences for controlling credential acquisition.  
 * As a result, KIM provides user preferences for initial credential options and 
 * user interface behavior such as the default client identity and the favorite identities list.
 *
 * \section kim_preferences_edit Viewing and Editing the Preferences
 * 
 * In order to view and edit the user's preferences, call #kim_preferences_create() to acquire a 
 * preferences object containing the user's preferences.  You can examine preferences
 * with the functions starting with "kim_preferences_get_" and change preferences with
 * the functions starting with "kim_preferences_set_".  Once you are done making changes,
 * you can write changes back out to the user's preferences with #kim_preferences_synchronize().
 *
 * \note The location of user preferences and the semantics of
 * preference synchronization is platform-specific.  Where possible KIM will use
 * platform-specific preference mechanisms.
 *
 * \section kim_preferences_options Initial Credential Options Preferences
 *
 * KIM provides user preferences for initial credential options.  These
 * are the options #kim_options_create() will use when creating a new KIM 
 * options object.  They are also the options specified by KIM_OPTIONS_DEFAULT.
 * You can view and edit the initial credential options using 
 * #kim_preferences_get_options() and #kim_preferences_set_options(). 
 *
 * \note Not all credential options in the kim_options_t object have corresponding 
 * user preferences.  For example, the prompt callback function is not stored
 * in the user preferences since it has no meaning outside of the current 
 * application.  Some options which are not currently stored in the
 * preferences may be stored there in the future. 
 *
 * If you are implementing a user interface for credentials acquisition, 
 * you should be aware that KIM has a user preference to manage the initial
 * credential options preferences. If the user successfully acquires credentials 
 * with non-default options and #kim_preferences_get_remember_options() is set 
 * to TRUE, you should store the options used to get credentials with 
 * #kim_preferences_set_options().  
 *
 * \section kim_preferences_client_identity Client Identity Preferences
 *
 * KIM also provides user preferences for the default client identity.   
 * This identity is used whenever KIM needs to display a graphical dialog for
 * credential acquisition but does not know what client identity to use.
 * You can view and edit the default client identity using 
 * #kim_preferences_get_client_identity() and 
 * #kim_preferences_set_client_identity(). 
 *
 * If you are implementing a user interface for credentials acquisition, 
 * you should be aware that KIM has a user preference to manage 
 * the client identity preferences. If the user successfully acquires credentials 
 * with non-default options and #kim_preferences_get_remember_client_identity() is  
 * set to TRUE, you should store the client identity for which credentials were
 * acquired using #kim_preferences_set_client_identity(). 
 * 
 * \section kim_preferences_favorite_identities Favorite Identities Preferences
 *
 * When presenting a graphical interface for credential acquisition, KIM 
 * may need to display a list of identities for the user to select from.  
 * This list is generated by the user's favorite identities preference.  
 * You can view and edit the favorite identities preference using 
 * #kim_preferences_get_favorite_identities() and 
 * #kim_preferences_set_favorite_identities().  Please see the
 * \ref kim_favorite_identities_overview for more information.
 * 
 * See \ref kim_preferences_reference for information on specific APIs.
 */

/*!
 * \defgroup kim_preferences_reference KIM Preferences Documentation
 * @{
 */

/*!
 * \param out_preferences on exit, a new preferences object.  
 *                        Must be freed with kim_preferences_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Create a new preferences object from the current user's preferences.
 */
kim_error_t kim_preferences_create (kim_preferences_t *out_preferences);

/*!
 * \param out_preferences on exit, a new preferences object which is a copy of in_preferences.  
 *                        Must be freed with kim_favorite_identities_free().
 * \param in_preferences  a preferences object. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Copy a preferences object.
 */
kim_error_t kim_preferences_copy (kim_preferences_t *out_preferences,
                                  kim_preferences_t  in_preferences);

/*!
 * \param io_preferences a preferences object to modify.
 * \param in_options     an options object.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the user's preferred options.
 * \sa kim_preferences_get_options()
 */
kim_error_t kim_preferences_set_options (kim_preferences_t io_preferences,
                                         kim_options_t     in_options);

/*!
 * \param in_preferences a preferences object.
 * \param out_options    on exit, the options specified in \a in_preferences.
 *                        Must be freed with kim_options_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the user's preferred options.
 * \sa kim_preferences_set_options()
 */
kim_error_t kim_preferences_get_options (kim_preferences_t  in_preferences,
                                         kim_options_t     *out_options);

/*!
 * \param io_preferences      a preferences object to modify.
 * \param in_remember_options a boolean value indicating whether or not to remember the last 
 *                            options used to acquire a credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to remember the last options the user used to acquire a credential.
 * \sa kim_preferences_get_remember_options()
 */
kim_error_t kim_preferences_set_remember_options (kim_preferences_t io_preferences,
                                                  kim_boolean_t     in_remember_options);

/*!
 * \param in_preferences       a preferences object.
 * \param out_remember_options on exit, a boolean value indicating whether or \a in_preferences will 
 *                             remember the last options used to acquire a credential.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to remember the last options the user used to acquire a credential.
 * \sa kim_preferences_set_remember_options()
 */
kim_error_t kim_preferences_get_remember_options (kim_preferences_t  in_preferences,
                                                  kim_boolean_t     *out_remember_options);

/*!
 * \param io_preferences      a preferences object to modify.
 * \param in_client_identity  a client identity object.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the user's preferred client identity.
 * \sa kim_preferences_get_client_identity()
 */
kim_error_t kim_preferences_set_client_identity (kim_preferences_t io_preferences,
                                                 kim_identity_t    in_client_identity);

/*!
 * \param in_preferences       a preferences object.
 * \param out_client_identity  on exit, the client identity specified in \a in_preferences.
 *                             Must be freed with kim_identity_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the user's preferred client identity.
 * \sa kim_preferences_set_client_identity()
 */
kim_error_t kim_preferences_get_client_identity (kim_preferences_t  in_preferences,
                                                 kim_identity_t    *out_client_identity);

/*!
 * \param io_preferences               a preferences object to modify.
 * \param in_remember_client_identity  a boolean value indicating whether or not to remember the last 
 *                                     client identity for which a credential was acquired.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set whether or not to remember the last client identity the user acquired a credential for.
 * \sa kim_preferences_get_remember_client_identity()
 */
kim_error_t kim_preferences_set_remember_client_identity (kim_preferences_t io_preferences,
                                                          kim_boolean_t     in_remember_client_identity);

/*!
 * \param in_preferences                a preferences object.
 * \param out_remember_client_identity  on exit, a boolean value indicating whether or \a in_preferences will 
 *                                      remember the last client identity for which a credential was acquired.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get whether or not to remember the last client identity the user acquired a credential for.
 * \sa kim_preferences_set_remember_client_identity()
 */
kim_error_t kim_preferences_get_remember_client_identity (kim_preferences_t  in_preferences,
                                                          kim_boolean_t     *out_remember_client_identity);

/*!
 * \param io_preferences       a preferences object to modify.
 * \param in_minimum_lifetime  a minimum lifetime indicating how small a lifetime the
 *                             GUI tools should allow the user to specify for credentials.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the minimum credential lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_get_minimum_lifetime()
 */
kim_error_t kim_preferences_set_minimum_lifetime (kim_preferences_t io_preferences,
                                                  kim_lifetime_t    in_minimum_lifetime);

/*!
 * \param in_preferences        a preferences object.
 * \param out_minimum_lifetime  on exit, the minimum lifetime that GUI tools will 
 *                              allow the user to specify for credentials.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the minimum credential lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_set_minimum_lifetime()
 */
kim_error_t kim_preferences_get_minimum_lifetime (kim_preferences_t  in_preferences,
                                                  kim_lifetime_t    *out_minimum_lifetime);

/*!
 * \param io_preferences       a preferences object to modify.
 * \param in_maximum_lifetime  a maximum lifetime indicating how large a lifetime the
 *                             GUI tools should allow the user to specify for credentials.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the maximum credential lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_get_maximum_lifetime()
 */
kim_error_t kim_preferences_set_maximum_lifetime (kim_preferences_t io_preferences,
                                                  kim_lifetime_t    in_maximum_lifetime);

/*!
 * \param in_preferences        a preferences object.
 * \param out_maximum_lifetime  on exit, the maximum lifetime that GUI tools will 
 *                              allow the user to specify for credentials.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the maximum credential lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_set_maximum_lifetime()
 */
kim_error_t kim_preferences_get_maximum_lifetime (kim_preferences_t  in_preferences,
                                                  kim_lifetime_t    *out_maximum_lifetime);

/*!
 * \param io_preferences               a preferences object to modify.
 * \param in_minimum_renewal_lifetime  a minimum lifetime indicating how small a lifetime the
 *                                     GUI tools should allow the user to specify for 
 *                                     credential renewal.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the minimum credential renewal lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_get_minimum_renewal_lifetime()
 */
kim_error_t kim_preferences_set_minimum_renewal_lifetime (kim_preferences_t io_preferences,
                                                          kim_lifetime_t    in_minimum_renewal_lifetime);

/*!
 * \param in_preferences                a preferences object.
 * \param out_minimum_renewal_lifetime  on exit, the minimum lifetime that GUI tools will 
 *                                      allow the user to specify for credential renewal.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the minimum credential renewal lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_set_minimum_renewal_lifetime()
 */
kim_error_t kim_preferences_get_minimum_renewal_lifetime (kim_preferences_t  in_preferences,
                                                          kim_lifetime_t    *out_minimum_renewal_lifetime);

/*!
 * \param io_preferences               a preferences object to modify.
 * \param in_maximum_renewal_lifetime  a maximum lifetime indicating how large a lifetime the
 *                                     GUI tools should allow the user to specify for 
 *                                     credential renewal.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the maximum credential renewal lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_get_minimum_renewal_lifetime()
 */
kim_error_t kim_preferences_set_maximum_renewal_lifetime (kim_preferences_t io_preferences,
                                                          kim_lifetime_t    in_maximum_renewal_lifetime);

/*!
 * \param in_preferences                a preferences object.
 * \param out_maximum_renewal_lifetime  on exit, the maximum lifetime that GUI tools will 
 *                                      allow the user to specify for credential renewal.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the maximum credential renewal lifetime for GUI credential lifetime controls.
 * \sa kim_preferences_set_minimum_renewal_lifetime()
 */
kim_error_t kim_preferences_get_maximum_renewal_lifetime (kim_preferences_t  in_preferences,
                                                          kim_lifetime_t    *out_maximum_renewal_lifetime);

/*!
 * \param io_preferences         a preferences object to modify.
 * \param in_favorite_identities a favorite identities object.
 *                           See \ref kim_favorite_identities_overview for more information on KIM
 *                           Favorite Identities. 
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Set the user's preferred list of identities.
 * \sa kim_preferences_get_favorite_identities()
 */
kim_error_t kim_preferences_set_favorite_identities (kim_preferences_t         io_preferences,
                                                     kim_favorite_identities_t in_favorite_identities);

/*!
 * \param in_preferences          a preferences object.
 * \param out_favorite_identities on exit, a copy of the favorite identities specified in \a in_preferences.
 *                                See \ref kim_favorite_identities_overview for more information on KIM
 *                                Favorite Identities.  Must be freed with kim_favorite_identities_free().
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Get the user's preferred list of identities.
 * \sa kim_preferences_set_favorite_identities()
 */
kim_error_t kim_preferences_get_favorite_identities (kim_preferences_t          in_preferences,
                                                     kim_favorite_identities_t *out_favorite_identities);

/*!
 * \param in_preferences a preferences object.
 * \return On success, #KIM_NO_ERROR.  On failure, an error object representing the failure.
 * \brief Synchronize a preferences object with the user's preferences, writing pending changes
 * and reading any changes applied by other processes.
 */
kim_error_t kim_preferences_synchronize (kim_preferences_t in_preferences);

/*!
 * \param io_preferences the preferences object to be freed.  Set to NULL on exit.
 * \brief Free memory associated with a preferences object.
 */
void kim_preferences_free (kim_preferences_t *io_preferences);

/*!@}*/

#ifdef __cplusplus
}
#endif

#endif /* KIM_PREFERENCES_H */
