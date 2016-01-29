/** @file
    @brief Header forward declaring MessageType and specifying a smart pointer.

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

#ifndef INCLUDED_MessageTypePtr_h_GUID_95F4FC92_63B2_4D8F_A13D_290855521ED8
#define INCLUDED_MessageTypePtr_h_GUID_95F4FC92_63B2_4D8F_A13D_290855521ED8

// Internal Includes
#include <osvr/Util/UniquePtr.h>

// Library/third-party includes
// - none

// Standard includes
// - none

struct OSVR_MessageTypeObject;
namespace osvr {
namespace connection {
    typedef OSVR_MessageTypeObject MessageType;
    /// @brief a uniquely-owned handle for holding a message type registration.
    typedef unique_ptr<MessageType> MessageTypePtr;
} // namespace connection
} // namespace osvr
#endif // INCLUDED_MessageTypePtr_h_GUID_95F4FC92_63B2_4D8F_A13D_290855521ED8
