#!/bin/bash

# Copyright (c) 2014 Benjamin Fleischer
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
# 3. Neither the name of the copyright holder nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.


function uninstall_osxfuse_main
{
    # Source libraries

    local library_path=""
    for library_path in "${BASH_SOURCE[0]%/*}/lib"/*.sh
    do
        if [[ -f "${library_path}" ]]
        then
            source "${library_path}" || return 1
        fi
    done

    common_log_initialize
    common_signal_trap_initialize

    # Uninstall core

    local core_version="`installer_package_get_info com.github.osxfuse.pkg.Core version`"

    if [[ -z "${core_version}" ]]
    then
        if [[ -e "/Library/Filesystems/osxfuse.fs" ]]
        then
            core_version="3.0"

        elif [[ -e "/Library/Filesystems/osxfusefs.fs" ]]
        then
            core_version="2.3"
        fi
    fi

    if [[ -n "${core_version}" ]]
    then
        if version_compare_ge "${core_version}" 3.0
        then
            macos_unload_kext "com.github.osxfuse.filesystems.osxfuse"
            osxfuse_uninstall_osxfuse_3_core

        elif version_compare_ge "${core_version}" 2.3
        then
            macos_unload_kext "com.github.osxfuse.filesystems.osxfusefs"
            osxfuse_uninstall_osxfuse_2_core
        fi
    fi

    # Uninstall preference pane

    local prefpane_version="`installer_package_get_info com.github.osxfuse.pkg.PrefPane version`"

    if [[ -z "${prefpane_version}" ]]
    then
        if [[ -e "/Library/PreferencePanes/OSXFUSE.prefPane" ]]
        then
            prefpane_version="3.0"
        fi
    fi

    if [[ -n "${prefpane_version}" ]]
    then
        if version_compare_ge "${prefpane_version}" 3.0
        then
            osxfuse_uninstall_osxfuse_3_prefpane

        elif version_compare_ge "${prefpane_version}" 2.3
        then
            osxfuse_uninstall_osxfuse_2_prefpane
        fi
    fi

    # Uninstall MacFUSE compatibility layer

    local macfuse_version="`installer_package_get_info com.github.osxfuse.pkg.MacFUSE version`"

    if [[ -z "${macfuse_version}" ]]
    then
        macfuse_version="`installer_package_get_info com.google.macfuse.core version`"
    fi

    if [[ -z "${macfuse_version}" ]]
    then
        if [[ -e /usr/local/lib/pkgconfig/macfuse.pc ]]
        then
            macfuse_version="3.0"

        elif [[ -e /usr/local/lib/libmacfuse_i32.2.dylib ]]
        then
            macfuse_version="2.3"
        fi
    fi

    if [[ -n "${macfuse_version}" ]]
    then
        if version_compare_ge "${macfuse_version}" 3.0
        then
            osxfuse_uninstall_osxfuse_3_macfuse

        elif version_compare_ge "${macfuse_version}" 2.3
        then
            osxfuse_uninstall_osxfuse_2_macfuse
        fi
    fi

    return 0
}

uninstall_osxfuse_main "${@}"
