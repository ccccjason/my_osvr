/** @file
    @brief Implementation header included by RegisterShutdownHandler.h on Win32
   platforms. WARNING: includes `windows.h`, can't prevent it!

    @date 2014

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef INCLUDED_RegisterShutdownHandlerWin32_h_GUID_4D1BA155_D70B_4BAD_92AF_F20D02D911AA
#define INCLUDED_RegisterShutdownHandlerWin32_h_GUID_4D1BA155_D70B_4BAD_92AF_F20D02D911AA

#ifdef OSVR_USE_WIN32_SHUTDOWN_HANDLER

// Internal Includes
// - none

// Library/third-party includes
// - none

// Standard includes
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace osvr {
namespace server {
#ifndef OSVR_DOXYGEN_EXTERNAL
    namespace detail {
        template <ShutdownHandler F>
        BOOL WINAPI handlerWrapper(DWORD signalType) {
            switch (signalType) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                F();
                return TRUE;
            default:
                return FALSE;
            }
        }
    }  // namespace detail
#endif // #ifndef OSVR_DOXYGEN_EXTERNAL

    /// @brief Register a function to be called when some attempt to close the
    /// console app occurs.
    ///
    /// @tparam F The address of a shutdown handler function: no params, no
    /// return value.
    template <ShutdownHandler F> inline void registerShutdownHandler() {
        SetConsoleCtrlHandler(&detail::handlerWrapper<F>, TRUE);
    }
} // namespace server
} // namespace osvr

#endif // OSVR_USE_WIN32_SHUTDOWN_HANDLER

#endif // INCLUDED_RegisterShutdownHandlerWin32_h_GUID_4D1BA155_D70B_4BAD_92AF_F20D02D911AA
