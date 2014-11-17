#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include <boost/program_options.hpp>
#include <Eigen/Dense>
#include <glog/logging.h>
#include <opencv2/opencv.hpp>

#include <camera.h>
#include <loader.h>
#include <sampling.h>
#include <scene.h>

using namespace pentatope;

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
        ("render", value<std::string>(), "run given RenderTask");
    variables_map vars;
    store(parse_command_line(argc, argv, desc), vars);
    notify(vars);

    if(vars.count("help") > 0) {
        std::cout << desc << std::endl;
    } else if(vars.count("render") > 0) {
        const auto task_path = vars["render"].as<std::string>();
        LOG(INFO) << "Render task path: " << task_path;
        auto task = loadTextProtoFile(task_path);
        auto scene = std::move(std::get<0>(task));
        auto camera = std::move(std::get<1>(task));
        auto sample_per_px = std::get<2>(task);
        const std::string output_path = "render.png";

        LOG(INFO) << "Starting task";
        Sampler sampler;
        cv::Mat result = camera->render(*scene, sampler, sample_per_px);
        LOG(INFO) << "Writing render result to " << output_path;
        cv::imwrite(output_path, result);
    } else {
        std::cout << desc << std::endl;
    }
    return 0;
}
