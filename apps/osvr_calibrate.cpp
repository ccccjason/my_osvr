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
#include "ClientMainloop.h"
#include "RecomposeTransform.h"
#include "WrapRoute.h"
#include <osvr/Server/ConfigureServerFromFile.h>
#include <osvr/Server/RegisterShutdownHandler.h>
#include <osvr/Common/JSONEigen.h>
#include <osvr/ClientKit/ClientKit.h>
#include <osvr/ClientKit/InterfaceStateC.h>

#include <osvr/Util/EigenInterop.h>

// Library/third-party includes
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <json/value.h>
#include <json/reader.h>

// Standard includes
#include <iostream>
#include <fstream>
#include <exception>

using std::cout;
using std::cerr;
using std::endl;

static osvr::server::ServerWeakPtr g_server;

auto SETTLE_TIME = boost::posix_time::seconds(2);

/// @brief Shutdown handler function - forcing the server pointer to be global.
void handleShutdown() {
    osvr::server::ServerPtr server(g_server.lock());
    if (server) {
        cout << "Received shutdown signal..." << endl;
        server->signalStop();
    } else {
        cout << "Received shutdown signal but server already stopped..."
             << endl;
    }
}

void waitForEnter() { std::cin.ignore(); }

inline Json::Value removeCalibration(std::string const &input) {
    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(input, root)) {
        throw std::runtime_error("Could not parse route");
    }
    return remove_levels_if(root, [](Json::Value const &current) {
        return current.isMember("calibration") &&
               current["calibration"].isBool() &&
               current["calibration"].asBool();
    });
}

int main(int argc, char *argv[]) {
    std::string configName;
    std::string outputName;
    namespace po = boost::program_options;
    // clang-format off
    po::options_description desc("Options");
    desc.add_options()
        ("help", "produce help message")
        ("route", po::value<std::string>()->default_value("/me/head"), "route to calibrate")
        ("output,O", po::value<std::string>(&outputName), "output file (defaults to same as config file)")
        ;
    po::options_description hidden("Hidden (positional-only) options");
    hidden.add_options()
        ("config", po::value<std::string>(&configName)->default_value(std::string(osvr::server::getDefaultConfigFilename())))
        ;
    // clang-format on

    po::positional_options_description p;
    p.add("config", 1);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
                  .options(po::options_description().add(desc).add(hidden))
                  .positional(p)
                  .run(),
              vm);
    po::notify(vm);

    {
        /// Deal with command line errors or requests for help
        bool usage = false;

        if (vm.count("help")) {
            usage = true;
        } else if (vm.count("route") != 1) {
            cout << "Error: --route is a required argument\n" << endl;
            usage = true;
        }

        if (usage) {
            cout << "Usage: osvr_calibrate [config file name] [options]"
                 << endl;
            cout << desc << "\n";
            return 1;
        }
    }

    if (outputName.empty()) {
        outputName = configName;
    }

    osvr::server::ServerPtr srv =
        osvr::server::configureServerFromFile(configName);
    if (!srv) {
        return -1;
    }
    g_server = srv;

    cout << "Registering shutdown handler..." << endl;
    osvr::server::registerShutdownHandler<&handleShutdown>();

    std::string dest = vm["route"].as<std::string>();
    std::string route = srv->getSource(dest);
    if (route.empty()) {
        cerr << "Error: No route found for provided destination: " << dest
             << endl;
        return -1;
    }
    cout << dest << " -> " << route << endl;
    Json::Value pruned = removeCalibration(route);
    Json::Value prunedDirective(Json::objectValue);
    {
        cout << pruned.toStyledString() << endl;
        cout << "Submitting cleaned route..." << endl;
        prunedDirective["destination"] = dest;
        prunedDirective["source"] = pruned;
        srv->addRoute(prunedDirective.toStyledString());
    }
    cout << "Starting client..." << endl;
    osvr::clientkit::ClientContext ctx("com.osvr.bundled.osvr_calibrate");
    osvr::clientkit::Interface iface = ctx.getInterface(dest);

    ClientMainloop client(ctx);
    srv->registerMainloopMethod([&client] { client.mainloop(); });
    {
        // Take ownership of the server inside this nested scope
        // We want to ensure that the client parts outlive the server.
        osvr::server::ServerPtr server(srv);
        srv.reset();

        cout << "Starting server and client mainloop..." << endl;
        server->start();
        cout << "Waiting a few seconds for the server to settle..." << endl;
        boost::this_thread::sleep(SETTLE_TIME);

        cout << "\n\nPlease place your device in its 'zero' orientation and "
                "press enter." << endl;
        waitForEnter();

        OSVR_OrientationState state;
        OSVR_TimeValue timestamp;
        OSVR_ReturnCode ret;
        {
            /// briefly interrupt the client mainloop so we can get stuff done
            /// with the client state.
            ClientMainloop::lock_type lock(client.getMutex());
            ret = osvrGetOrientationState(iface.get(), &timestamp, &state);
        }
        if (ret != OSVR_RETURN_SUCCESS) {
            cerr << "Sorry, no orientation state available for this route - "
                    "are you sure you have a device plugged in and your path "
                    "correct?" << endl;
            return -1;
        }
        Eigen::AngleAxisd rotation(osvr::util::fromQuat(state).inverse());

        cout << "Angle: " << rotation.angle()
             << " Axis: " << rotation.axis().transpose() << endl;
        Json::Value newRoute;
        {
            Json::Value newLayer(Json::objectValue);
            newLayer["calibration"] = true;
            newLayer["rotate"]["radians"] = rotation.angle();
            newLayer["rotate"]["axis"] = osvr::common::toJson(rotation.axis());
            newRoute = wrapRoute(prunedDirective, newLayer);
            bool isNew = server->addRoute(newRoute.toStyledString());
            BOOST_ASSERT_MSG(
                !isNew,
                "Server claims this is a new, rather than a replacement, "
                "route... should not happen!");
        }

        /// Give the server and client time to react, and to get their console
        /// prints out of the way.
        boost::this_thread::sleep(SETTLE_TIME);

        cout << "\n\nNew calibration applied: please inspect it with the "
                "Tracker Viewer." << endl;
        cout << "(If rotations appear incorrect, you may first need to add "
                "a basisChange transform layer to the route.)" << endl;
        if (configName == outputName) {
            cout << "If you are satisfied and want to OVERWRITE your existing "
                    "config file with this update, press y." << endl;
            cout << "Otherwise, press enter or Ctrl-C to break out of this "
                    "program.\n" << endl;
            cout << "Overwrite '" << configName << "'? [yN] ";
            char confirm;
            std::cin.get(confirm);
            cout << endl;
            if (confirm != 'y' && confirm != 'Y') {
                cout << "Calibration save cancelled." << endl;
                return 1;
            }
        }
        Json::Value root;
        {
            std::ifstream config(configName);
            if (!config.good()) {
                cerr << "Could not read the original config file again!"
                     << endl;
                return -1;
            }

            Json::Reader reader;
            if (!reader.parse(config, root)) {
                cerr << "Could not parse the original config file again! "
                        "Should never happen!" << endl;
                return -1;
            }
        }
        auto &routes = root["routes"];
        for (auto &fileRoute : routes) {
            if (fileRoute["destination"] == dest) {
                fileRoute = newRoute;
            }
        }
        {
            cout << "\n\nWriting updated config file to " << outputName << endl;
            std::ofstream outfile(outputName);
            outfile << root.toStyledString();
        }

        cout << "Awaiting Ctrl-C to trigger server shutdown..." << endl;
        server->awaitShutdown();
    }
    cout << "Server mainloop exited." << endl;

    return 0;
}
