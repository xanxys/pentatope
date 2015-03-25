#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <boost/network/protocol/http/server.hpp>
#include <boost/program_options.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>
#include <unistd.h>

#include <camera.h>
#include <loader.h>
#include <sampling.h>
#include <scene.h>

using namespace pentatope;

namespace http = boost::network::http;

class RenderHandler;
using http_server = http::server<RenderHandler>;

class RenderHandler {
public:
    void operator()(
            const http_server::request& request,
            http_server::response& response) {
        response = http_server::response::stock_reply(
            http_server::response::ok, "Hello");
    }

    void log(const http_server::string_type& info) {
        LOG(WARNING) << "server: " << info;
    }
};



// Since there is a python frontend for pentatope binary,
// error messages can be unhelpful to reduce code clutter.
int main(int argc, char** argv) {
    using boost::program_options::notify;
    using boost::program_options::options_description;
    using boost::program_options::parse_command_line;
    using boost::program_options::store;
    using boost::program_options::value;
    using boost::program_options::variables_map;

    google::InitGoogleLogging(argv[0]);
    google::InstallFailureSignalHandler();

    options_description desc("Renderer for 4-d space");
    desc.add_options()
        ("help", "show this message")
        ("render", value<std::string>(), "run given RenderTask (either text or binary)")
        ("max-threads", value<int>(), "Maximum number of worker threads (default: nproc).");
    variables_map vars;
    store(parse_command_line(argc, argv, desc), vars);
    notify(vars);

    if(vars.count("help") > 0) {
        std::cout << desc << std::endl;
    } else if(vars.count("render") > 0) {
        const auto task_path = vars["render"].as<std::string>();
        LOG(INFO) << "Render task path: " << task_path;
        auto task = loadProtoFile(task_path);
        auto scene = std::move(std::get<0>(task));
        const auto camera = std::move(std::get<1>(task));
        const auto sample_per_px = std::get<2>(task);
        const auto output_path = std::get<3>(task);

        // Calculate number of threads to use.
        int n_threads = sysconf(_SC_NPROCESSORS_ONLN);
        if(vars.count("max-threads") > 0) {
            const int max_threads = vars["max-threads"].as<int>();
            CHECK_GT(max_threads, 0) << "Need a positive number of cores to proceed";
            n_threads = std::min(max_threads, n_threads);
        }
        LOG(INFO) << "Using #threads=" << n_threads;

        LOG(INFO) << "Starting task";
        Sampler sampler;
        cv::Mat result = camera->render(*scene, sampler,
            sample_per_px, n_threads);
        LOG(INFO) << "Writing render result to " << output_path;
        cv::imwrite(output_path, result);
    } else {
        LOG(INFO) << "Running as an HTTP service, listening on port 80";

        RenderHandler handler;
        http_server server(
            http_server::options(handler)
                .address("0.0.0.0")
                .port("80")
                .reuse_address(true));
        server.run();
    }
    return 0;
}
