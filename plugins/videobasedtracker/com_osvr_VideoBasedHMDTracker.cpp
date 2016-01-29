/** @file
    @brief Main file for a video-based HMD tracker.

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

// Internal Includes
#include "VideoBasedTracker.h"
#include "HDKLedIdentifierFactory.h"
#include "CameraParameters.h"
#include "ImageSource.h"
#include "ImageSourceFactories.h"
#include <osvr/PluginKit/PluginKit.h>
#include <osvr/PluginKit/TrackerInterfaceC.h>
#include <osvr/PluginKit/AnalogInterfaceC.h>
#include "HDKData.h"

#include "ConfigurationParser.h"

// Generated JSON header file
#include "com_osvr_VideoBasedHMDTracker_json.h"

// Library/third-party includes
#include <opencv2/core/core.hpp> // for basic OpenCV types
#include <opencv2/core/operations.hpp>
#include <opencv2/highgui/highgui.hpp> // for image capture
#include <opencv2/imgproc/imgproc.hpp> // for image scaling
#include <json/value.h>
#include <json/reader.h>

#include <boost/noncopyable.hpp>

// Standard includes
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>

// Define the constant below to print timing information (how many updates
// per second we are getting).
//#define VBHMD_TIMING

// Define the constant below to set a directory to save the video frames that
// are acquired with files in a format that can later be read by
// VBHMD_FAKE_IMAGES
//#define VBHMD_SAVE_IMAGES "./Frames"

// Anonymous namespace to avoid symbol collision
namespace {

static const auto DEBUGGABLE_BEACONS = 34;
static const auto DATAPOINTS_PER_BEACON = 5;

class VideoBasedHMDTracker : boost::noncopyable {
  public:
    VideoBasedHMDTracker(OSVR_PluginRegContext ctx,
                         osvr::vbtracker::ImageSourcePtr &&source,
                         int devNumber = 0,
                         osvr::vbtracker::ConfigParams const &params =
                             osvr::vbtracker::ConfigParams{})
        : m_source(std::move(source)), m_vbtracker(params), m_params(params) {
        if (params.numThreads > 0) {
            // Set the number of threads for OpenCV to use.
            cv::setNumThreads(params.numThreads);
        }

        /// Create the initialization options
        OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);

        // Configure the tracker interface.
        osvrDeviceTrackerConfigure(opts, &m_tracker);

        osvrDeviceAnalogConfigure(opts, &m_analog,
                                  DEBUGGABLE_BEACONS * DATAPOINTS_PER_BEACON);

        /// Come up with a device name
        std::ostringstream os;
        os << "TrackedCamera" << devNumber << "_" << 0;

        /// Create an asynchronous (threaded) device
        m_dev.initAsync(ctx, os.str(), opts);

        /// Send JSON descriptor
        m_dev.sendJsonDescriptor(com_osvr_VideoBasedHMDTracker_json);

        /// Register update callback
        m_dev.registerUpdateCallback(this);
    }

    OSVR_ReturnCode update();

    /// Provides access to the underlying video-based tracker object to add
    /// sensors.
    osvr::vbtracker::VideoBasedTracker &vbtracker() { return m_vbtracker; }

  private:
    osvr::pluginkit::DeviceToken m_dev;
    OSVR_TrackerDeviceInterface m_tracker;
    OSVR_AnalogDeviceInterface m_analog;
    osvr::vbtracker::ImageSourcePtr m_source;
    osvr::vbtracker::ConfigParams const m_params;
#ifdef VBHMD_SAVE_IMAGES
    int m_imageNum = 1;
#endif
    cv::Mat m_frame;
    cv::Mat m_imageGray;

    osvr::vbtracker::VideoBasedTracker m_vbtracker;
};

inline OSVR_ReturnCode VideoBasedHMDTracker::update() {
    if (!m_source->ok()) {
        // Couldn't open the camera.  Failing silently for now. Maybe the
        // camera will be plugged back in later.
        return OSVR_RETURN_SUCCESS;
    }

    //==================================================================
    // Trigger a camera grab.
    if (!m_source->grab()) {
        // Couldn't open the camera.  Failing silently for now. Maybe the
        // camera will be plugged back in later.
        return OSVR_RETURN_SUCCESS;
    }

    //==================================================================
    // Keep track of when we got the image, since that is our
    // best estimate for when the tracker was at the specified
    // pose.
    // TODO: Back-date the aquisition time by the expected image
    // transfer time and perhaps by half the exposure time to say
    // when the photons actually arrived.
    OSVR_TimeValue timestamp;
    osvrTimeValueGetNow(&timestamp);
    // Pull the image into an OpenCV matrix named m_frame.
    m_source->retrieve(m_frame, m_imageGray);

#ifdef VBHMD_SAVE_IMAGES
    // If we're supposed to save images, make file names that match the
    // format we need to read them back in again and save the images.
    std::ostringstream fileName;
    fileName << VBHMD_SAVE_IMAGES << "/";
    fileName << std::setfill('0') << std::setw(4) << m_imageNum++;
    fileName << ".tif";
    if (!cv::imwrite(fileName.str(), m_frame)) {
        std::cerr << "Could not write image to " << fileName.str() << std::endl;
    }

#endif

#ifdef VBHMD_TIMING
    //==================================================================
    // Time our performance
    static struct timeval last = {0, 0};
    if (last.tv_sec == 0) {
        vrpn_gettimeofday(&last, NULL);
    }
    static unsigned count = 0;
    if (++count == 100) {
        struct timeval now;
        vrpn_gettimeofday(&now, NULL);
        double duration = vrpn_TimevalDurationSeconds(now, last);
        std::cout << "Video-based tracker: update rate " << count / duration
                  << " hz" << std::endl;
        count = 0;
        last = now;
    }
#endif
    bool shouldSendDebug = false;
    m_vbtracker.processImage(
        m_frame, m_imageGray, timestamp,
        [&](OSVR_ChannelCount sensor, OSVR_Pose3 const &pose) {

            //==================================================================
            // Report the new pose, time-stamped with the time we
            // received the image from the camera.
            osvrDeviceTrackerSendPoseTimestamped(m_dev, m_tracker, &pose,
                                                 sensor, &timestamp);
            if (sensor == 0) {
                shouldSendDebug = true;
            }
        });
    if (shouldSendDebug && m_params.streamBeaconDebugInfo) {
        double data[DEBUGGABLE_BEACONS * DATAPOINTS_PER_BEACON];
        auto &debug = m_vbtracker.getFirstEstimator().getBeaconDebugData();
        auto now = osvr::util::time::getNow();
        auto n = std::min(size_t(DEBUGGABLE_BEACONS), debug.size());
        for (std::size_t i = 0; i < n; ++i) {
            double *buf = &data[i];
            auto j = i * DATAPOINTS_PER_BEACON;
            // yes, using postincrement since we want the previous value
            // returned. Borderline "too clever" but it's debug code.
            data[j] = debug[i].variance;
            data[j + 1] = debug[i].measurement.x;
            data[j + 2] = debug[i].measurement.y;
            data[j + 3] = debug[i].residual.x;
            data[j + 4] = debug[i].residual.y;
        }
        osvrDeviceAnalogSetValuesTimestamped(m_dev, m_analog, data, n, &now);
    }

    return OSVR_RETURN_SUCCESS;
}

class HardwareDetection {
  public:
    using CameraFactoryType = std::function<osvr::vbtracker::ImageSourcePtr()>;
    using SensorSetupType = std::function<void(VideoBasedHMDTracker &)>;
    HardwareDetection(CameraFactoryType camFactory, SensorSetupType setup,
                      int cameraID = 0,
                      osvr::vbtracker::ConfigParams const &params =
                          osvr::vbtracker::ConfigParams{})
        : m_found(false), m_cameraFactory(camFactory), m_sensorSetup(setup),
          m_cameraID(cameraID), m_params(params) {}

    OSVR_ReturnCode operator()(OSVR_PluginRegContext ctx) {
        if (m_found) {
            return OSVR_RETURN_SUCCESS;
        }
        auto src = m_cameraFactory();
        if (!src || !src->ok()) {
            return OSVR_RETURN_FAILURE;
        }
        m_found = true;

        /// Create our device object, passing the context and moving the camera.
        std::cout << "Opening camera " << m_cameraID << std::endl;
        auto newTracker = osvr::pluginkit::registerObjectForDeletion(
            ctx, new VideoBasedHMDTracker(ctx, std::move(src), m_cameraID,
                                          m_params));
        m_sensorSetup(*newTracker);
        return OSVR_RETURN_SUCCESS;
    }

  private:
    /// @brief Have we found our device yet? (this limits the plugin to one
    /// instance, so that only one tracker will use this camera.)
    bool m_found = false;

    CameraFactoryType m_cameraFactory;
    SensorSetupType m_sensorSetup;

    int m_cameraID; //< Which OpenCV camera should we open?
    osvr::vbtracker::ConfigParams const m_params;
};

/// @name Helper functions for loading calibration files
/// @{
inline cv::Point3f parsePoint(Json::Value const &jsonArray) {
    return cv::Point3f(jsonArray[0].asFloat(), jsonArray[1].asFloat(),
                       jsonArray[2].asFloat());
}

inline std::vector<cv::Point3f>
parseArrayOfPoints(Json::Value const &jsonArray) {
    /// in case of error, we just return an empty array.
    std::vector<cv::Point3f> ret;
    if (!jsonArray.isArray()) {
        return ret;
    }
    for (auto &entry : jsonArray) {

        if (!entry.isArray() || entry.size() != 3) {
            ret.clear();
            return ret;
        }
        ret.emplace_back(parsePoint(entry));
    }
    return ret;
}

inline std::vector<cv::Point3f>
tryLoadingArrayOfPointsFromFile(std::string const &filename) {
    std::vector<cv::Point3f> ret;
    if (filename.empty()) {
        return ret;
    }
    Json::Value root;
    {
        std::ifstream calibfile(filename);
        if (!calibfile.good()) {
            return ret;
        }
        Json::Reader reader;
        if (!reader.parse(calibfile, root)) {
            return ret;
        }
    }
    ret = parseArrayOfPoints(root);
    return ret;
}
/// @}

class ConfiguredDeviceConstructor {
  public:
    /// @brief This is the required signature for a device instantiation
    /// callback.
    OSVR_ReturnCode operator()(OSVR_PluginRegContext ctx, const char *params) {
        // Read the JSON data from parameters.
        Json::Value root;
        if (params) {
            Json::Reader r;
            if (!r.parse(params, root)) {
                std::cerr << "Could not parse parameters!" << std::endl;
            }
        }

        // Read these parameters from a "params" field in the device Json
        // configuration file.

        // Using `get` here instead of `[]` lets us provide a default value.
        int cameraID = root.get("cameraID", 0).asInt();

        // This is in a separate function/header foro sharing and for clarity.
        auto config = osvr::vbtracker::parseConfigParams(root);

        /// Functions to indicate which beacons should be considered "fixed" -
        /// not autocalibrated.
        auto backPanelFixedBeacon = [](int) { return true; };
        auto frontPanelFixedBeacon = [](int id) {
            return (id == 16) || (id == 17) || (id == 19) || (id == 20);
        };

        /// @todo get this (and the path) from the config file
        bool fakeImages = false;
        if (fakeImages) {
            /// Immediately create a "fake images" tracker.
            auto path = std::string{};
            // fake images
            auto src = osvr::vbtracker::openImageFileSequence(path);
            if (!src) {
                return OSVR_RETURN_FAILURE;
            }
            auto newTracker = osvr::pluginkit::registerObjectForDeletion(
                ctx, new VideoBasedHMDTracker(ctx, std::move(src), cameraID,
                                              config));
            auto camParams = osvr::vbtracker::getSimulatedHDKCameraParameters();
            newTracker->vbtracker().addSensor(
                osvr::vbtracker::createHDKLedIdentifierSimulated(0), camParams,
                osvr::vbtracker::OsvrHdkLedLocations_SENSOR0,
                osvr::vbtracker::OsvrHdkLedDirections_SENSOR0,
                frontPanelFixedBeacon, 4, 2);
            // There are sometimes only four beacons on the back unit (two of
            // the LEDs are disabled), so we let things work with just those.
            newTracker->vbtracker().addSensor(
                osvr::vbtracker::createHDKLedIdentifierSimulated(1), camParams,
                osvr::vbtracker::OsvrHdkLedLocations_SENSOR1,
                osvr::vbtracker::OsvrHdkLedDirections_SENSOR1,
                backPanelFixedBeacon, 4, 0);
            return OSVR_RETURN_SUCCESS;
        }
#if 0
        bool isOculusCamera = (width == 376) && (height == 480);
#endif

#ifdef _WIN32
        /// @todo speed of a function pointer here vs a lambda?
        auto cameraFactory = &osvr::vbtracker::openHDKCameraDirectShow;

#else // !_WIN32
        /// @todo This is rather crude, as we can't select the exact camera we
        /// want, nor set the "50Hz" high-gain mode (and only works with HDK
        /// camera firmware v7 and up). Presumably eventually use libuvc on
        /// other platforms instead, at least for the HDK IR camera.

        auto cameraFactory = [=] {
            return osvr::vbtracker::openOpenCVCamera(cameraID);
        };
#endif

        /// Function to execute after the device is created, to add the sensors.
        std::function<void(VideoBasedHMDTracker & newTracker)>
            setupHDKParamsAndSensors;
        /// Loading a calibration file means our beacon locations are better
        /// known than we might otherwise expect.
        auto BEACON_AUTOCALIB_ERROR_SCALE_IF_CALIBRATED = 0.1;
        if (config.includeRearPanel) {
            // distance between front and back panel target origins, in mm.
            auto distanceBetweenPanels = config.headCircumference / M_PI * 10. +
                                         config.headToFrontBeaconOriginDistance;
            setupHDKParamsAndSensors =
                [config, distanceBetweenPanels, frontPanelFixedBeacon,
                 BEACON_AUTOCALIB_ERROR_SCALE_IF_CALIBRATED](
                    VideoBasedHMDTracker &newTracker) {
                    osvr::vbtracker::Point3Vector locations =
                        osvr::vbtracker::OsvrHdkLedLocations_SENSOR0;
                    osvr::vbtracker::Vec3Vector directions =
                        osvr::vbtracker::OsvrHdkLedDirections_SENSOR0;
                    std::vector<double> variances =
                        osvr::vbtracker::OsvrHdkLedVariances_SENSOR0;

                    // For the back panel beacons: have to rotate 180 degrees
                    // about Y, which is the same as flipping sign on X and Z
                    // then we must translate along Z by head diameter +
                    // distance from head to front beacon origins
                    for (auto &pt :
                         osvr::vbtracker::OsvrHdkLedLocations_SENSOR1) {
                        locations.emplace_back(-pt.x, pt.y,
                                               -pt.z - distanceBetweenPanels);
                        variances.push_back(config.backPanelMeasurementError);
                    }
                    // Similarly, rotate the directions.
                    for (auto &vec :
                         osvr::vbtracker::OsvrHdkLedDirections_SENSOR1) {
                        directions.emplace_back(-vec[0], vec[1], -vec[2]);
                    }
                    double autocalibScale = 1;
                    auto calibLocations =
                        tryLoadingArrayOfPointsFromFile(config.calibrationFile);
                    if (calibLocations.size() == locations.size()) {
                        std::cout << "Video-based tracker: Successfully loaded "
                                     "beacon calibration file "
                                  << config.calibrationFile << std::endl;
                        locations = calibLocations;
                        autocalibScale =
                            BEACON_AUTOCALIB_ERROR_SCALE_IF_CALIBRATED;
                    } else if (!config.calibrationFile.empty()) {
                        std::cout
                            << "Video-based tracker: NOTE: Beacon calibration "
                               "filename "
                            << config.calibrationFile
                            << " was specified, but not found or could not "
                               "be loaded."
                            << std::endl;
                    }

                    auto camParams = osvr::vbtracker::getHDKCameraParameters();
                    newTracker.vbtracker().addSensor(
                        osvr::vbtracker::createHDKUnifiedLedIdentifier(),
                        camParams, locations, directions, variances,
                        frontPanelFixedBeacon, 4, 0, autocalibScale);
                };
        } else {
            // OK, so if we don't have to include the rear panel as part of the
            // single sensor, that's easy.
            setupHDKParamsAndSensors =
                [frontPanelFixedBeacon, config, backPanelFixedBeacon,
                 BEACON_AUTOCALIB_ERROR_SCALE_IF_CALIBRATED](
                    VideoBasedHMDTracker &newTracker) {
                    auto camParams = osvr::vbtracker::getHDKCameraParameters();

                    auto calibLocations =
                        tryLoadingArrayOfPointsFromFile(config.calibrationFile);
                    if (calibLocations.size() ==
                        osvr::vbtracker::OsvrHdkLedLocations_SENSOR0.size()) {
                        std::cout << "Video-based tracker: Successfully loaded "
                                     "beacon calibration file "
                                  << config.calibrationFile << std::endl;

                        newTracker.vbtracker().addSensor(
                            osvr::vbtracker::createHDKLedIdentifier(0),
                            camParams, calibLocations,
                            osvr::vbtracker::OsvrHdkLedDirections_SENSOR0,
                            osvr::vbtracker::OsvrHdkLedVariances_SENSOR0,
                            frontPanelFixedBeacon, 6, 0,
                            BEACON_AUTOCALIB_ERROR_SCALE_IF_CALIBRATED);
                    } else {
                        if (!config.calibrationFile.empty()) {
                            std::cout
                                << "Video-based tracker: NOTE: Beacon "
                                   "calibration filename "
                                << config.calibrationFile
                                << " was specified, but not found or could not "
                                   "be loaded."
                                << std::endl;
                        }
                        newTracker.vbtracker().addSensor(
                            osvr::vbtracker::createHDKLedIdentifier(0),
                            camParams,
                            osvr::vbtracker::OsvrHdkLedLocations_SENSOR0,
                            osvr::vbtracker::OsvrHdkLedDirections_SENSOR0,
                            osvr::vbtracker::OsvrHdkLedVariances_SENSOR0,
                            frontPanelFixedBeacon, 6, 0);
                    }
                    newTracker.vbtracker().addSensor(
                        osvr::vbtracker::createHDKLedIdentifier(1), camParams,
                        osvr::vbtracker::OsvrHdkLedLocations_SENSOR1,
                        osvr::vbtracker::OsvrHdkLedDirections_SENSOR1,
                        backPanelFixedBeacon, 4, 0);
                };
        }

        // OK, now that we have our parameters, create the device.
        osvr::pluginkit::PluginContext context(ctx);
        context.registerHardwareDetectCallback(new HardwareDetection(
            cameraFactory, setupHDKParamsAndSensors, cameraID, config));

        return OSVR_RETURN_SUCCESS;
    }
};

} // namespace

OSVR_PLUGIN(com_osvr_VideoBasedHMDTracker) {
    osvr::pluginkit::PluginContext context(ctx);

    /// Tell the core we're available to create a device object.
    osvr::pluginkit::registerDriverInstantiationCallback(
        ctx, "VideoBasedHMDTracker", new ConfiguredDeviceConstructor);

    return OSVR_RETURN_SUCCESS;
}
