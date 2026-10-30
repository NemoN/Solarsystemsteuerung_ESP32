#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/dietmar-sensor-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/tests/mocks" -I"$project_dir/Backload" \
  "$project_dir/Backload/SensorReading.cpp" "$project_dir/tests/test_sensor_readings.cpp" \
  -o "$test_dir/sensor-tests"
"$test_dir/sensor-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" \
  "$project_dir/Backload/PumpControl.cpp" "$project_dir/tests/test_pump_control.cpp" \
  -o "$test_dir/pump-tests"
"$test_dir/pump-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" \
  "$project_dir/Backload/AutomaticControl.cpp" "$project_dir/Backload/PumpControl.cpp" "$project_dir/tests/test_automatic_control.cpp" \
  -o "$test_dir/automatic-tests"
"$test_dir/automatic-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" \
  "$project_dir/Backload/TemperatureHistory.cpp" "$project_dir/tests/test_temperature_history.cpp" \
  -o "$test_dir/history-tests"
"$test_dir/history-tests"
