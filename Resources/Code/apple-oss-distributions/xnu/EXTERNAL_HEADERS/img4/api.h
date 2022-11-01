/*!
 * @header
 * API definitions.
 */
#ifndef __IMG4_API_H
#define __IMG4_API_H

#ifndef __IMG4_INDIRECT
#error "Please #include <img4/firmware.h> instead of this file directly"
#endif // __IMG4_INDIRECT

#include <img4/shim.h>

/*!
 * @const IMG4_API_VERSION
 * The API version of the library. This version will be changed in accordance
 * with new API introductions so that callers may submit code to the build that
 * adopts those new APIs before the APIs land by using the following pattern:
 *
 *     #if IMG4_API_VERSION >= 20180424
 *     img4_new_api();
 *     #endif
 *
 * In this example, the library maintainer and API adopter agree on an API
 * version of 20180424 ahead of time for the introduction of
 * img4_new_api(). When a libdarwin with that API version is submitted, the
 * project is rebuilt, and the new API becomes active.
 *
 * Breaking API changes will be both covered under this mechanism as well as
 * individual preprocessor macros in this header that declare new behavior as
 * required.
 */
#define IMG4_API_VERSION (20212105u)

#if IMG4_TARGET_DARWIN
#define IMG4_API_AVAILABLE_FALL_2018 \
		API_AVAILABLE( \
			macos(10.15), \
			ios(12.0), \
			tvos(12.0), \
			watchos(5.0))

#define IMG4_API_DEPRECATED_FALL_2018 \
		API_DEPRECATED_WITH_REPLACEMENT( \
			"img4_firmware_t", \
			macos(10.15, 11.0), \
			ios(12.2, 14.0), \
			tvos(12.2, 14.0), \
			watchos(5.2, 7.0))

#define IMG4_API_AVAILABLE_SPRING_2019 \
		API_AVAILABLE(\
			macos(10.15), \
			ios(12.2), \
			tvos(12.2), \
			watchos(5.2))

#define IMG4_API_AVAILABLE_FALL_2020 \
		API_AVAILABLE( \
			macos(11.0), \
			ios(14.0), \
			tvos(14.0), \
			watchos(7.0), \
			bridgeos(5.0))

#define IMG4_API_AVAILABLE_FALL_2021 \
		API_AVAILABLE( \
			macos(12.0), \
			ios(15.0), \
			tvos(15.0), \
			watchos(8.0), \
			bridgeos(6.0))
#else
#define IMG4_API_AVAILABLE_FALL_2018
#define IMG4_API_DEPRECATED_FALL_2018
#define IMG4_API_AVAILABLE_SPRING_2019
#define IMG4_API_AVAILABLE_FALL_2020
#define IMG4_API_AVAILABLE_FALL_2021
#endif

#define IMG4_API_AVAILABLE_20180112 IMG4_API_AVAILABLE_FALL_2018
#define IMG4_API_AVAILABLE_20181106 IMG4_API_AVAILABLE_SPRING_2019
#define IMG4_API_AVAILABLE_20200508 IMG4_API_AVAILABLE_FALL_2020
#define IMG4_API_AVAILABLE_20200608 IMG4_API_AVAILABLE_FALL_2020
#define IMG4_API_AVAILABLE_20200724 IMG4_API_AVAILABLE_FALL_2020
#define IMG4_API_AVAILABLE_20210113 IMG4_API_AVAILABLE_FALL_2021
#define IMG4_API_AVAILABLE_20210205 IMG4_API_AVAILABLE_FALL_2021
#define IMG4_API_AVAILABLE_20210226 IMG4_API_AVAILABLE_FALL_2021
#define IMG4_API_AVAILABLE_20210305 IMG4_API_AVAILABLE_FALL_2021
#define IMG4_API_AVAILABLE_20210521 IMG4_API_AVAILABLE_FALL_2021

/*!
 * @typedef img4_struct_version_t
 * A type describing the version of a structure in the library.
 */
IMG4_API_AVAILABLE_20180112
typedef uint16_t img4_struct_version_t;

#endif // __IMG4_API_H
