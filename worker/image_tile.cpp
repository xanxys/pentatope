#include "image_tile.h"

#include <cmath>
#include <string>
#include <vector>

#include <camera.h>


namespace pentatope {

void setImageTileFrom(const cv::Mat& image, ImageTile& tile) {
    std::vector<uint8_t> buffer;

    // LDR image for compatibility.
    cv::imencode(".png", Camera2::tonemapLinear(image), buffer);
    tile.set_blob_png(std::string(buffer.begin(), buffer.end()));

    // Decompose into floating point number components.
    cv::Mat log_temp;
    cv::log(image, log_temp);
    cv::Mat exponent = cv::min(255, cv::max(0, log_temp / std::log(2.0) + 127));
    cv::Mat int_temp;
    exponent.convertTo(int_temp, CV_8UC3);
    exponent = int_temp;
    exponent.convertTo(log_temp, CV_32FC3);

    cv::Mat integral_temp;
    cv::Mat exponent_base_e = (log_temp - 127) * std::log(2.0);
    cv::exp(exponent_base_e , integral_temp);
    cv::Mat mantissa = cv::min(255, cv::max(0, image / integral_temp - 1) * 256);

    cv::Mat t;
    exponent.convertTo(t, CV_8UC3);
    cv::imencode(".png", t, buffer);
    tile.set_blob_png_exponent(std::string(buffer.begin(), buffer.end()));

    mantissa.convertTo(t, CV_8UC3);
    cv::imencode(".png", t, buffer);
    tile.set_blob_png_mantissa(std::string(buffer.begin(), buffer.end()));
}

}  // namespace
