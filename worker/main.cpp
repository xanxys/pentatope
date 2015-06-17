#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#include <boost/network/protocol/http/server.hpp>
#include <boost/program_options.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>
#include <unistd.h>

#include <camera.h>
#include <image_tile.h>
#include <loader.h>
#include <proto/render_server.pb.h>
#include <proto/render_task.pb.h>
#include <sampling.h>
#include <scene.h>

using namespace pentatope;


cv::Mat executeRenderTask(const int n_threads, const RenderTask& rtask) {
    auto task = loadRenderTask(rtask);
    auto scene = std::move(std::get<0>(task));
    const auto camera = std::move(std::get<1>(task));
    const auto sample_per_px = std::get<2>(task);

    LOG(INFO) << "Starting task";
    Sampler sampler;
    return camera->render(*scene, sampler, sample_per_px, n_threads);
}


namespace http = boost::network::http;

class RenderHandler;
using http_server = http::server<RenderHandler>;

class RenderHandler {
public:
    RenderHandler(int n_threads) : n_threads(n_threads) {
        assert(n_threads > 0);
    }

    void operator()(
            const http_server::request& request,
            http_server::response& response) {
        if(request.method != "POST") {
            response = http_server::response::stock_reply(
                http_server::response::bad_request,
                "Use POST method");
            return;
        }
        RenderRequest render_request;
        if(!render_request.ParseFromString(body(request))) {
            response = http_server::response::stock_reply(
                http_server::response::bad_request,
                "Use render_server.RenderRequest protobuf");
            return;
        }

        // process something
        LOG(INFO) << "Processing RenderRequest";
        RenderResponse render_response;

        processRequest(render_request, render_response);

        std::string response_body;
        if(!render_response.SerializeToString(&response_body)) {
            response = http_server::response::stock_reply(
                http_server::response::internal_server_error,
                "Somehow failed to serialize render_server.RenderResponse protobuf");
            return;
        }
        response = http_server::response::stock_reply(
            http_server::response::ok, response_body);
    }

    void log(const http_server::string_type& message) {
        LOG(WARNING) << "server: " << message;
    }
private:
    void processRequest(const RenderRequest& request, RenderResponse& response) noexcept {
        if(!request.has_task()) {
            response.set_status(RenderResponse::RENDERING_ERROR);
            response.set_error_message("Nothing to do");
            return;
        }

        if(!request.task().has_scene() && !request.has_scene_id()) {
            // No way to get scene.
            response.set_status(RenderResponse::SCENE_UNAVAILABLE);
            return;
        }

        // Read/write cache if the request has scene_id.
        RenderTask cached_task(request.task());
        if(request.has_scene_id()) {
            if(!updateTaskAndCache(request.scene_id(), request.task(), cached_task)) {
                response.set_status(RenderResponse::SCENE_UNAVAILABLE);
                return;
            }
        }

        const cv::Mat result_hdr = executeRenderTask(n_threads, cached_task);
        setImageTileFrom(result_hdr, *response.mutable_output_tile());
        response.set_status(RenderResponse::SUCCESS);
    }

    bool updateTaskAndCache(uint64_t scene_id, const RenderTask& in_task, RenderTask& cached_task) {
        std::lock_guard<std::mutex> lock(scene_cache_mutex);

        if(in_task.has_scene()) {
            // Update cache.
            scene_cache[scene_id] = in_task.scene();
            return true;
        }
        // Try getting from cache.
        const auto maybe_scene = scene_cache.find(scene_id);
        if(maybe_scene != scene_cache.end()) {
            cached_task.mutable_scene()->CopyFrom(maybe_scene->second);
            return true;
        }
        // Failed to get cache, and scene is not spcified.
        return false;
    }
private:
    std::mutex scene_cache_mutex;
    // TODO: consider saving loaded Scene to avoid errors and save computation.
    std::map<uint64_t, RenderScene> scene_cache;

    const int n_threads;
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
        ("output", value<std::string>(), "write output to given path (only works with --render)")
        ("max-threads", value<int>(), "Maximum number of worker threads (default: nproc).");
    variables_map vars;
    store(parse_command_line(argc, argv, desc), vars);
    notify(vars);

    // Calculate number of threads to use.
    int n_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if(vars.count("max-threads") > 0) {
        const int max_threads = vars["max-threads"].as<int>();
        CHECK_GT(max_threads, 0) << "Need a positive number of cores to proceed";
        n_threads = std::min(max_threads, n_threads);
    }
    LOG(INFO) << "Using #threads=" << n_threads;

    if(vars.count("help") > 0) {
        std::cout << desc << std::endl;
    } else if(vars.count("render") > 0) {
        if(vars.count("output") == 0) {
            std::cout << "--output is required for --render" << std::endl;
            std::cout << desc << std::endl;
            return -1;
        }
        const auto task_path = vars["render"].as<std::string>();
        LOG(INFO) << "Render task path: " << task_path;
        auto task = readRenderTaskFromFile(task_path);

        const auto output_path = vars["output"].as<std::string>();
        const cv::Mat result = executeRenderTask(n_threads, task);
        LOG(INFO) << "Writing render result to " << output_path;
        cv::imwrite(output_path, result);
    } else {
        if(vars.count("output") > 0) {
            LOG(WARNING) << "Service mode ignores --output";
        }
        LOG(INFO) << "Running as an HTTP service, listening on port 80";
        RenderHandler handler(n_threads);
        http_server server(
            http_server::options(handler)
                .address("0.0.0.0")
                .port("80")
                .reuse_address(true));
        server.run();
    }
    return 0;
}
