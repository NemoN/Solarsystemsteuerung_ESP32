#!/bin/sh
set -eu

project_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/dietmar-sensor-tests.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined -DBACKLOAD_HOST_TEST \
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
  "$project_dir/Backload/PumpRuntime.cpp" "$project_dir/tests/test_pump_runtime.cpp" \
  -o "$test_dir/pump-runtime-tests"
"$test_dir/pump-runtime-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" \
  "$project_dir/Backload/AutomaticControl.cpp" "$project_dir/Backload/PumpControl.cpp" "$project_dir/tests/test_automatic_control.cpp" \
  -o "$test_dir/automatic-tests"
"$test_dir/automatic-tests"
python3 "$project_dir/tests/test_automatic_settings.py"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" \
  "$project_dir/Backload/TemperatureHistory.cpp" "$project_dir/tests/test_temperature_history.cpp" \
  -o "$test_dir/history-tests"
"$test_dir/history-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" "$project_dir/tests/test_recovery_ap_policy.cpp" \
  -o "$test_dir/recovery-ap-tests"
"$test_dir/recovery-ap-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -DBACKLOAD_HOST_TEST \
  -I"$project_dir/tests/web_access_mocks" \
  "$project_dir/tests/test_web_access.cpp" -o "$test_dir/web-access-tests"
"$test_dir/web-access-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -I"$project_dir/Backload" "$project_dir/tests/test_hostname.cpp" \
  -o "$test_dir/hostname-tests"
"$test_dir/hostname-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" "$project_dir/tests/test_status_led.cpp" \
  -o "$test_dir/status-led-tests"
"$test_dir/status-led-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/Backload" "$project_dir/tests/test_solar_control.cpp" \
  -o "$test_dir/solar-tests"
"$test_dir/solar-tests"
python3 "$project_dir/tests/test_solar_integration.py"

python3 "$project_dir/tests/test_heating_integration.py"
