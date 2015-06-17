#pragma once

#include <opencv2/opencv.hpp>

#include <proto/render_server.pb.h>

namespace pentatope {

void setImageTileFrom(const cv::Mat& image, ImageTile& tile);

}  // namespace
