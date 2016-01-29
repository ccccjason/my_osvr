/** @file
    @brief Implementation

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

// Internal Includes
#include <osvr/ClientKit/Context.h>
#include <osvr/ClientKit/Parameters.h>

// Library/third-party includes
// - none

// Standard includes
#include <iostream>

int main() {
    osvr::clientkit::ClientContext context(
        "com.osvr.exampleclients.DisplayParameter");

    std::string displayDescription = context.getStringParameter("/display");

    std::cout << "Got value of /display:\n" << displayDescription << std::endl;

    std::cout << "Library shut down, exiting." << std::endl;
    return 0;
}
