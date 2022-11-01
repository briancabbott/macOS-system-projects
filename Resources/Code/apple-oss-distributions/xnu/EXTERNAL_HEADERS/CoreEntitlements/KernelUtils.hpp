//
//  KernelUtils.hpp
//  CoreEntitlements_kernel
//
//

#pragma once

#include "CoreEntitlements.h"
#include <libkern/c++/OSPtr.h>
#include <libkern/c++/OSDictionary.h>

#ifndef CORE_ENTITLEMENTS_I_KNOW_WHAT_IM_DOING
#error This is a private API, please consult with the Trusted Execution team before using this. Misusing these functions will lead to security issues.
#endif

/*!
 * @function CEQueryContextToOSDictionary
 * Private API, converts a query context into an OSDictionary that can be handed out to legacy users
 */
OSPtr<OSDictionary> CEQueryContextToOSDictionary(CEQueryContext_t entitlements);
