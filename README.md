# bag2video

将 ROS bag 中的压缩图像话题 `/baton/image_left/compressed` 转为 MP4 视频，编码优先 H.265（HEVC），次选 H.264。

## 依赖
- ROS1（已测试 noetic）：`roscpp`、`rosbag`、`sensor_msgs`
- OpenCV（带 FFmpeg 后端）
- CMake、g++

## 构建
```bash
source /opt/ros/noetic/setup.bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

## 运行
```bash
cd build
./bag2video \
  --bag /root/lwx_dataset/uav/record_sensor_6.bag \
  --topic /baton/image_left/compressed \
  --fps 25 \
  --out output.mp4 \
  --codec h265
```
- 如环境不支持 HEVC，可改 `--codec h264`。  
- 输出文件位于 `build/output.mp4`。

## 快捷测试脚本
```bash
./test_bag2video.sh
```
脚本会自动构建并运行，默认使用 H.264 编码，输出到 `build/output.mp4`，如有 `ffprobe` 会显示视频信息。

## 参数说明
- `--bag`  bag 文件路径（必填）
- `--topic` 图像话题名，默认 `/baton/image_left/compressed`
- `--fps`   输出帧率，默认 25
- `--out`   输出文件名，默认 `output.mp4`
- `--codec` `h265`/`h264`/`auto`，`auto` 表示先尝试 H.265 再 H.264
