#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"
BAG_PATH="/root/lwx_dataset/uav/record_sensor_6.bag"
TOPIC="/baton/image_right/compressed"
FPS=25
OUT_FILE="${BUILD_DIR}/output.mp4"
CODEC="h264"

if [ -f /opt/ros/noetic/setup.bash ]; then
  source /opt/ros/noetic/setup.bash
elif [ -f /opt/ros/melodic/setup.bash ]; then
  source /opt/ros/melodic/setup.bash
else
  echo "Warning: ROS environment not found; build may fail." >&2
fi

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake .. >/dev/null
make -j"$(nproc)" >/dev/null

./bag2video --bag "${BAG_PATH}" --topic "${TOPIC}" --fps "${FPS}" --out "${OUT_FILE}" --codec "${CODEC}"

if command -v ffprobe >/dev/null; then
  echo "ffprobe summary:"
  ffprobe -v error -select_streams v:0 -show_entries stream=codec_name,width,height,nb_frames,avg_frame_rate -of default=nw=1:nk=1 "${OUT_FILE}"
fi

ls -lh "${OUT_FILE}"
