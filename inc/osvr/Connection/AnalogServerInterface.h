/** @file
    @brief Header

    @date 2015

    @author
    Sensics, Inc.
    <http://sensics.com/osvr>
*/

// Copyright 2015 Sensics, Inc.
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

#ifndef INCLUDED_AnalogServerInterface_h_GUID_C64EDC9F_8073_4682_7415_5623C4C2129C
#define INCLUDED_AnalogServerInterface_h_GUID_C64EDC9F_8073_4682_7415_5623C4C2129C

// Internal Includes
#include <osvr/Util/ChannelCountC.h>
#include <osvr/Util/ClientReportTypesC.h>
#include <osvr/Util/TimeValue.h>

// Library/third-party includes
// - none

// Standard includes
// - none

namespace osvr {
namespace connection {
    /// @brief Interface for external access to generating analog reports.
    ///
    /// Does not inherit from BaseServerInterface because it wraps an existing
    /// VRPN class.
    class AnalogServerInterface {
      public:
        typedef OSVR_AnalogState value_type;
        /// @brief Sets the value to val at channel chan, and reports changes
        /// with the given timestamp
        virtual bool setValue(value_type val, OSVR_ChannelCount chan,
                              util::time::TimeValue const &timestamp) = 0;

        /// @brief Sets the values from the val array at channels [0, chans),
        /// and reports changes with the given timestamp
        virtual void setValues(value_type val[], OSVR_ChannelCount chans,
                               util::time::TimeValue const &timestamp) = 0;
    };

} // namespace connection
} // namespace osvr

#endif // INCLUDED_AnalogServerInterface_h_GUID_C64EDC9F_8073_4682_7415_5623C4C2129C
