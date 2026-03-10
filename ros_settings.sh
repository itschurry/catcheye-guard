############## ROS2 SETTINGS ##############
alias sr="source ~/ros_ws/ros_settings.sh"
alias ri="sudo -E rosdep install --from-paths src --ignore-src -r -y"
alias cb="colcon build --parallel-workers 4 --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release"
alias cbp="colcon build --parallel-workers 4 --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Release --packages-up-to"

source /opt/ros/$ROS_DISTRO/setup.bash
if [ -f "$HOME/ros_ws/install/setup.bash" ]; then
    source "$HOME/ros_ws/install/setup.bash"
fi
source /usr/share/colcon_cd/function/colcon_cd.sh
export _colcon_cd_root=/opt/ros/$ROS_DISTRO/
source /usr/share/colcon_argcomplete/hook/colcon-argcomplete.bash
# source /usr/share/vcstool-completion/vcs.bash

# export CMAKE_PREFIX_PATH=$CMAKE_PREFIX_PATH:/opt/ros/humble
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib:/usr/lib

# export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/.ros/fastdds.xml
# export ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}                 # 0 ~ 101 정수 사용 가능
# HOST_NS="$(hostname | tr '[:upper:]' '[:lower:]' | tr '-' '_' )"
# export ROS_NAMESPACE="/${HOST_NS}"

# export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
# export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
# export RMW_IMPLEMENTATION=rmw_connext_cpp
# export RMW_IMPLEMENTATION=rmw_gurumdds_cpp

# 로그 레벨 설정
export RCUTILS_LOGGING_BUFFERED_STREAM=0
# export RCUTILS_LOGGING_MIN_SEVERITY=DEBUG
export RCUTILS_LOGGING_DEFAULT_LOGGER_LEVEL=DEBUG
export RCUTILS_COLORIZED_OUTPUT=1
export RCUTILS_LOGGING_USE_STDOUT=0
export LAUNCH_LOGGING_FORMAT='[%(levelname)s %(asctime)s] [%(name)s]: %(message)s'
export LAUNCH_LOGGING_DATE_FORMAT='%Y-%m-%d %H:%M:%S.%f%z'
# export RCUTILS_CONSOLE_OUTPUT_FORMAT="[{severity}] [{name}]: {message}"
# export RCUTILS_CONSOLE_OUTPUT_FORMAT='[{severity} {time}] [{name}]: {message} ({function_name}() at {file_name}:{line_number})'
export RCUTILS_CONSOLE_OUTPUT_FORMAT='[{severity} {time}] [{name}]: {message}'

## 날짜 기반(YYYY-MM-DD) 로그 폴더 생성
# KST(Asia/Seoul) 기준으로 날짜를 고정합니다. 필요 시 환경변수 TZ로 오버라이드 가능합니다.
TODAY=$(TZ=${TZ:-Asia/Seoul} date +'%Y-%m-%d')

# 기본 로그 루트는 $HOME/farmily_ws/farmily_logs 이며, LOG_BASE_DIR로 오버라이드 가능합니다.
LOG_BASE_DIR="${LOG_BASE_DIR:-$HOME/robot_logs}"
LOG_DIR="${LOG_BASE_DIR}/${TODAY}"

# 폴더가 없으면 생성, 있으면 그대로 통과
mkdir -p "$LOG_DIR"
export ROS_LOG_DIR="$LOG_DIR"

echo "[ros_settings] ROS_LOG_DIR set to $ROS_LOG_DIR"