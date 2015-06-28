#include "image_tile.h"

#include <cmath>
#include <string>
#include <vector>

#include <boost/range/irange.hpp>

#include <camera.h>


namespace pentatope {

void setImageTileFrom(const cv::Mat& image, ImageTile& tile) {
    std::vector<uint8_t> buffer;

    // LDR image for compatibility.
    cv::imencode(".png", Camera2::tonemapLinear(image), buffer);
    tile.set_blob_png(std::string(buffer.begin(), buffer.end()));

    // Decompose into floating point number components.
    cv::Mat mantissa(image.rows, image.cols, CV_8UC3);
    cv::Mat exponent(image.rows, image.cols, CV_8UC3);
    for(int y : boost::irange(0, image.rows)) {
        for(int x : boost::irange(0, image.cols)) {
            const cv::Vec3f v = image.at<cv::Vec3f>(y, x);
            const auto p0 = decomposeFloat(v[0]);
            const auto p1 = decomposeFloat(v[1]);
            const auto p2 = decomposeFloat(v[2]);

            mantissa.at<cv::Vec3b>(y, x) = cv::Vec3b(p0.first, p1.first, p2.first);
            exponent.at<cv::Vec3b>(y, x) = cv::Vec3b(p0.second, p1.second, p2.second);
        }
    }

    cv::imencode(".png", mantissa, buffer);
    tile.set_blob_png_mantissa(std::string(buffer.begin(), buffer.end()));

    cv::imencode(".png", exponent, buffer);
    tile.set_blob_png_exponent(std::string(buffer.begin(), buffer.end()));
}

// Decompose a float into mantissa and exponent.
std::pair<uint8_t, uint8_t> decomposeFloat(float v) {
    if(v <= 0) {
        // Approximate by the smallest normal number.
        return std::make_pair(0, 0);
    }
    int exponent;
    float fract = std::frexp(v, &exponent);
    fract *= 2;
    exponent -= 1;
    assert(1 <= fract && fract < 2);

    return std::make_pair(
        std::min(255, std::max(0, static_cast<int>((fract - 1) * 256))),
        std::min(255, std::max(0, exponent + 127)));
}

}  // namespace
