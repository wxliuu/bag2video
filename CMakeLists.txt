cmake_minimum_required(VERSION 3.10)
project(bag2video)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  rosbag
  sensor_msgs
)

find_package(OpenCV REQUIRED)

include_directories(
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
)

add_executable(bag2video
  src/bag2video.cpp
)

target_link_libraries(bag2video
  ${catkin_LIBRARIES}
  ${OpenCV_LIBRARIES}
)
