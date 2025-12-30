#include <rosbag/bag.h>
#include <rosbag/view.h>
#include <sensor_msgs/CompressedImage.h>

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <boost/foreach.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <chrono>
#include <filesystem>

struct Options {
  std::string bag_path;
  std::string topic = "/baton/image_left/compressed";
  double fps = 25.0;
  std::string output = "output.mp4";
  std::string codec = "auto";  // auto => try H265 first then H264
};

void printUsage() {
  std::cout << "Usage: bag2video --bag <bag_file> [--topic <topic>] [--fps <fps>] "
               "[--out <output.mp4>] [--codec h265|h264|auto]\n";
}

bool parseArgs(int argc, char** argv, Options& opt) {
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--bag" && i + 1 < argc) {
      opt.bag_path = argv[++i];
    } else if (arg == "--topic" && i + 1 < argc) {
      opt.topic = argv[++i];
    } else if (arg == "--fps" && i + 1 < argc) {
      opt.fps = std::stod(argv[++i]);
    } else if (arg == "--out" && i + 1 < argc) {
      opt.output = argv[++i];
    } else if (arg == "--codec" && i + 1 < argc) {
      opt.codec = argv[++i];
    } else if (arg == "--help" || arg == "-h") {
      printUsage();
      return false;
    } else {
      std::cerr << "Unknown or incomplete argument: " << arg << "\n";
      printUsage();
      return false;
    }
  }
  if (opt.bag_path.empty()) {
    std::cerr << "Error: --bag is required.\n";
    printUsage();
    return false;
  }
  if (opt.fps <= 0.0) {
    std::cerr << "Error: --fps must be positive.\n";
    return false;
  }
  if (opt.codec != "auto" && opt.codec != "h265" && opt.codec != "h264" &&
      opt.codec != "H265" && opt.codec != "H264") {
    std::cerr << "Error: --codec must be auto, h265, or h264.\n";
    return false;
  }
  return true;
}

int fourccFromString(const std::string& s) {
  if (s.size() != 4) return 0;
  return cv::VideoWriter::fourcc(s[0], s[1], s[2], s[3]);
}

bool tryOpenWriter(cv::VideoWriter& writer, const cv::Size& size, double fps,
                   const std::string& filename, const std::vector<std::string>& codec_list,
                   std::string& chosen_codec) {
  for (const auto& c : codec_list) {
    int cc = fourccFromString(c);
    if (cc == 0) continue;
    writer.open(filename, cc, fps, size, true);
    if (writer.isOpened()) {
      chosen_codec = c;
      return true;
    }
  }
  return false;
}

int main(int argc, char** argv) {
  Options opt;
  if (!parseArgs(argc, argv, opt)) return 1;

  // Prepare codec preference lists
  std::vector<std::string> hevc_codes = {"HEVC", "H265", "X265", "hvc1", "hev1"};
  std::vector<std::string> h264_codes = {"H264", "X264", "avc1"};

  std::vector<std::string> codec_try;
  if (opt.codec == "h265" || opt.codec == "H265") {
    codec_try = hevc_codes;
  } else if (opt.codec == "h264" || opt.codec == "H264") {
    codec_try = h264_codes;
  } else {  // auto: prefer H265 then H264
    codec_try.insert(codec_try.end(), hevc_codes.begin(), hevc_codes.end());
    codec_try.insert(codec_try.end(), h264_codes.begin(), h264_codes.end());
  }

  if (!std::filesystem::exists(opt.bag_path)) {
    std::cerr << "Bag file not found: " << opt.bag_path << "\n";
    return 1;
  }

  rosbag::Bag bag;
  try {
    bag.open(opt.bag_path, rosbag::bagmode::Read);
  } catch (const rosbag::BagException& e) {
    std::cerr << "Failed to open bag: " << e.what() << "\n";
    return 1;
  }

  std::vector<std::string> topics{opt.topic};
  rosbag::View view(bag, rosbag::TopicQuery(topics));
  if (view.size() == 0) {
    std::cerr << "No messages found on topic " << opt.topic << "\n";
    rosbag::View all(bag);
    std::set<std::string> available;
    for (const rosbag::ConnectionInfo* c : all.getConnections()) {
      available.insert(c->topic);
    }
    if (!available.empty()) {
      std::cerr << "Available topics in bag:\n";
      for (const auto& t : available) {
        std::cerr << "  " << t << "\n";
      }
    }
    bag.close();
    return 1;
  }

  cv::VideoWriter writer;
  std::string chosen_codec;
  cv::Size frame_size;
  bool first_frame = true;
  size_t written = 0;
  size_t skipped_decode = 0;

  auto start = std::chrono::steady_clock::now();

  for (const rosbag::MessageInstance& m : view) {
    sensor_msgs::CompressedImageConstPtr img_msg = m.instantiate<sensor_msgs::CompressedImage>();
    if (!img_msg) continue;

    cv::Mat buffer(1, static_cast<int>(img_msg->data.size()), CV_8UC1,
                   const_cast<uint8_t*>(img_msg->data.data()));
    cv::Mat image = cv::imdecode(buffer, cv::IMREAD_COLOR);
    if (image.empty()) {
      ++skipped_decode;
      continue;
    }

    if (first_frame) {
      frame_size = image.size();
      if (!tryOpenWriter(writer, frame_size, opt.fps, opt.output, codec_try, chosen_codec)) {
        std::cerr << "Failed to open VideoWriter with requested codecs (H265 then H264 only).\n";
        bag.close();
        return 1;
      }
      std::cout << "VideoWriter opened with codec " << chosen_codec << ", size "
                << frame_size.width << "x" << frame_size.height << ", fps " << opt.fps << "\n";
      first_frame = false;
    }

    if (image.size() != frame_size) {
      cv::resize(image, image, frame_size);
    }

    writer.write(image);
    ++written;
  }

  writer.release();
  bag.close();

  auto end = std::chrono::steady_clock::now();
  double seconds = std::chrono::duration<double>(end - start).count();

  if (written == 0) {
    std::cerr << "No frames written. Check bag and topic.\n";
    return 1;
  }

  std::cout << "Finished writing " << written << " frames to " << opt.output
            << " using codec " << chosen_codec << " in " << seconds << " s.\n";
  if (skipped_decode > 0) {
    std::cout << "Skipped frames (decode failed): " << skipped_decode << "\n";
  }

  return 0;
}
