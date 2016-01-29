/** @file
    @brief Implementation

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
// 	http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include <osvr/ClientKit/Context.h>
#include <osvr/Common/ClientContext.h>
#include <osvr/Common/PathTreeFull.h>
#include <osvr/Common/ResolveFullTree.h>
#include <osvr/Util/TreeTraversalVisitor.h>
#include <osvr/Common/PathElementTypes.h>
#include <osvr/Common/PathElementTools.h>
#include <osvr/Common/ApplyPathNodeVisitor.h>
#include <osvr/Common/PathNode.h>
#include <osvr/Util/IndentingStream.h>
#include <osvr/Util/ProgramOptionsToggleFlags.h>

// Library/third-party includes
#include <boost/program_options.hpp>
#include <boost/noncopyable.hpp>
#include <boost/variant.hpp>

// Standard includes
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

struct Options {
    bool showAliasSource;
    bool showAliasPriority;
    bool showDeviceDetails;
    bool showDeviceDescriptor;
    bool showSensors;
    bool showStringData;
};

class TreeNodePrinter : public boost::static_visitor<>, boost::noncopyable {
  public:
    /// @brief Constructor
    TreeNodePrinter(Options opts)
        : boost::static_visitor<>(), m_opts(opts),
          m_maxTypeLen(osvr::common::elements::getMaxTypeNameLength()),
          m_os(std::cout), m_indentStream{m_maxTypeLen + 2 + 1 + 2, m_os} {
        // Some initial space to set the output off.
        m_os << "\n\n";

        // Computation for initializing the indent stream above:
        // Indents type name size, +2 for the brackets, +1 for the space, and +2
        // so it doesn't line up with the path.
    }

    /// @brief print nothing for a null element.
    void operator()(osvr::common::PathNode const &,
                    osvr::common::elements::NullElement const &) {}

    /// @brief We might print something for a sensor element.
    void operator()(osvr::common::PathNode const &node,
                    osvr::common::elements::SensorElement const &elt) {
        if (m_opts.showSensors) {
            m_outputBasics(node, elt) << "\n";
        }
    }

    /// @brief Print aliases
    void operator()(osvr::common::PathNode const &node,
                    osvr::common::elements::AliasElement const &elt) {
        m_outputBasics(node, elt) << std::endl;
        if (m_opts.showAliasSource) {
            m_indentStream << "-> " << elt.getSource() << std::endl;
        }
        if (m_opts.showAliasPriority) {
            m_indentStream << "Priority: " << osvr::common::outputPriority(
                                                  elt.priority()) << std::endl;
        }
    }
    /// @brief Print Devices
    void operator()(osvr::common::PathNode const &node,
                    osvr::common::elements::DeviceElement const &elt) {
        m_outputBasics(node, elt) << std::endl;
        if (m_opts.showDeviceDetails) {
            m_indentStream << "- corresponds to " << elt.getFullDeviceName()
                           << std::endl;
        }
        if (m_opts.showDeviceDescriptor) {
            m_indentStream << "- Descriptor: "
                           << elt.getDescriptor().toStyledString() << std::endl;
        }
    }

    /// @brief We might print something for a sensor element.
    void operator()(osvr::common::PathNode const &node,
                    osvr::common::elements::StringElement const &elt) {
        m_outputBasics(node, elt) << std::endl;
        if (m_opts.showStringData) {
            m_indentStream << "- Contained value: " << elt.getString()
                           << std::endl;
        }
    }

    /// @brief Catch-all for other element types.
    template <typename T>
    void operator()(osvr::common::PathNode const &node, T const &elt) {
        m_outputBasics(node, elt) << "\n";
    }

  private:
    /// @brief shared implementation
    template <typename T>
    std::ostream &m_outputBasics(osvr::common::PathNode const &node,
                                 T const &elt) {
        m_os << "[" << std::setw(m_maxTypeLen) << osvr::common::getTypeName(elt)
             << "] " << osvr::common::getFullPath(node);
        return m_os;
    }
    Options m_opts;
    size_t m_maxTypeLen;
    std::ostream &m_os;
    osvr::util::IndentingStream m_indentStream;
};

int main(int argc, char *argv[]) {
    Options opts;
    namespace po = boost::program_options;
    // clang-format off
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "produce help message")
        ("show-alias-source", po::value<bool>(&opts.showAliasSource)->default_value(true), "Whether or not to show the source associated with each alias")
        ("show-alias-priority", po::value<bool>(&opts.showAliasPriority)->default_value(false), "Whether or not to show the priority associated with each alias")
        ("show-device-details", po::value<bool>(&opts.showDeviceDetails)->default_value(true), "Whether or not to show the basic details associated with each device")
        ("show-device-descriptors", po::value<bool>(&opts.showDeviceDescriptor)->default_value(false), "Whether or not to show the JSON descriptors associated with each device")
        ("show-sensors", po::value<bool>(&opts.showSensors)->default_value(true), "Whether or not to show the 'sensor' nodes")
        ("show-string-data", po::value<bool>(&opts.showStringData)->default_value(true), "Whether or not to show the data in 'string' nodes")
        ;
    // clang-format on
    po::variables_map vm;
    bool usage = false;
    try {
        po::store(po::command_line_parser(argc, argv)
                      .options(desc)
                      .extra_parser(osvr::util::convertHideIntoFalseShow)
                      .run(),
                  vm);
        po::notify(vm);
    } catch (std::exception &e) {
        std::cerr << "\nError parsing command line: " << e.what() << "\n\n";
        usage = true;
    }
    if (usage || vm.count("help")) {
        std::cerr
            << "\nTraverses the path tree and outputs it as text for "
               "human consumption. See\n"
               "PathTreeExport for structured output for graphical display.\n";
        std::cerr << "Usage: " << argv[0] << " [options]\n\n";
        std::cerr << "All --show options have a matching --hide option.\n\n";
        std::cerr << desc << "\n";
        return 1;
    }

    osvr::common::PathTree pathTree;
    {
        /// We only actually need the client open for long enough to get the
        /// path tree and clone it.
        osvr::clientkit::ClientContext context("com.osvr.tools.printtree");

        if (!context.checkStatus()) {
            std::cerr << "Client context has not yet started up - waiting. Make sure the server is running." << std::endl;
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                context.update();
            } while (!context.checkStatus());
            std::cerr << "OK, client context ready. Proceeding." << std::endl;
        }
        /// Get a non-const copy of the path tree.
        osvr::common::clonePathTree(context.get()->getPathTree(), pathTree);
        /// Resolve all aliases
        osvr::common::resolveFullTree(pathTree);
    }

    TreeNodePrinter printer{opts};
    /// Now traverse for output
    osvr::util::traverseWith(
        pathTree.getRoot(), [&printer](osvr::common::PathNode const &node) {
            osvr::common::applyPathNodeVisitor(printer, node);
        });

    return 0;
}