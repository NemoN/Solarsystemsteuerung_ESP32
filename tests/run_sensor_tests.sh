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
  "$project_dir/tests/test_history_server_url.cpp" -o "$test_dir/history-server-url-tests"
"$test_dir/history-server-url-tests"
python3 "$project_dir/tests/test_history_server_settings.py"
node "$project_dir/tests/test_history_server_ui.js"
node "$project_dir/tests/test_sensor_status_ui.js"

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

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  "$project_dir/tests/test_remote_relay_state.cpp" -o "$test_dir/remote-relay-tests"
"$test_dir/remote-relay-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  "$project_dir/tests/test_remote_relay_protocol.cpp" -o "$test_dir/remote-relay-protocol-tests"
"$test_dir/remote-relay-protocol-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/tests/relay_ota_mocks" \
  "$project_dir/Relaiskarte/RelayOta.cpp" "$project_dir/tests/test_relay_ota.cpp" \
  -o "$test_dir/relay-ota-tests"
"$test_dir/relay-ota-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/tests/relay_ota_mocks" -I"$project_dir/Backload" \
  "$project_dir/Backload/RemoteRelayUpdate.cpp" "$project_dir/Relaiskarte/RelayOta.cpp" \
  "$project_dir/tests/test_remote_relay_update_integration.cpp" -o "$test_dir/relay-ota-integration-tests"
"$test_dir/relay-ota-integration-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  "$project_dir/tests/test_relay_discovery_list.cpp" -o "$test_dir/relay-discovery-tests"
"$test_dir/relay-discovery-tests"

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$project_dir/tests/relay_ota_mocks" -I"$project_dir/Backload" \
  "$project_dir/Backload/RemoteRelay.cpp" "$project_dir/tests/test_remote_relay_multiple.cpp" -o "$test_dir/remote-multiple-tests"
"$test_dir/remote-multiple-tests"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -DMOCK_PREF_FAILURE -I"$project_dir/tests/relay_ota_mocks" -I"$project_dir/Backload" \
  "$project_dir/Backload/RemoteRelay.cpp" "$project_dir/tests/test_remote_relay_forget.cpp" -o "$test_dir/remote-forget-tests"
"$test_dir/remote-forget-tests"
"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  "$project_dir/tests/test_relay_factory_reset.cpp" -o "$test_dir/relay-factory-reset-tests"
"$test_dir/relay-factory-reset-tests"
python3 "$project_dir/tests/test_relay_pair_import.py"
python3 "$project_dir/tests/test_relay_pair_button.py"
python3 "$project_dir/tests/test_relay_pair_connect.py"
python3 "$project_dir/tests/test_relay_guide.py"
python3 "$project_dir/tests/test_remote_relay_update_modes.py"
if [ -n "${BACKLOAD_FIRMWARE_BIN:-}" ]; then
  python3 "$project_dir/tests/test_built_dashboard.py" "$BACKLOAD_FIRMWARE_BIN"
fi

# Host-HMAC- und Kopplungstests mit optional installiertem mbedcrypto.
if command -v pkg-config >/dev/null && pkg-config --exists mbedcrypto; then
  cmp "$project_dir/Backload/RelayPairAuth.h" "$project_dir/Relaiskarte/RelayPairAuth.h"
  cmp "$project_dir/Backload/RelayPairSecure.h" "$project_dir/Relaiskarte/RelayPairSecure.h"
  pair_crypto_private_flag=
  if [ ! -f "$(pkg-config --variable=includedir mbedcrypto)/mbedtls/ecdh.h" ]; then
    pair_crypto_private_flag=-DRELAY_PAIR_MBEDTLS_PRIVATE
  fi
  # Word splitting is intentional for pkg-config compiler/linker arguments.
  "${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -DMBEDTLS_DECLARE_PRIVATE_IDENTIFIERS \
    $(pkg-config --cflags mbedcrypto) "$project_dir/tests/test_relay_pair_auth.cpp" \
    $(pkg-config --libs mbedcrypto) -o "$test_dir/relay-pair-auth-tests"
  "$test_dir/relay-pair-auth-tests"
  "${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -DMBEDTLS_DECLARE_PRIVATE_IDENTIFIERS $pair_crypto_private_flag \
    -I"$project_dir/tests/relay_ota_mocks" $(pkg-config --cflags mbedcrypto) \
    "$project_dir/tests/test_relay_pair_secure.cpp" $(pkg-config --libs mbedcrypto) -o "$test_dir/relay-pair-secure-tests"
  "$test_dir/relay-pair-secure-tests"
  "${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -DBACKLOAD_HOST_TEST -DMBEDTLS_DECLARE_PRIVATE_IDENTIFIERS $pair_crypto_private_flag \
    -I"$project_dir/tests/relay_ota_mocks" $(pkg-config --cflags mbedcrypto) -I"$project_dir/Backload" \
    "$project_dir/Backload/RemoteRelay.cpp" "$project_dir/Backload/RelayProvisioning.cpp" \
    "$project_dir/tests/test_relay_provisioning.cpp" $(pkg-config --libs mbedcrypto) -o "$test_dir/relay-provisioning-tests"
  "$test_dir/relay-provisioning-tests"
else
  printf '%s\n' 'SKIP: Kopplungs- und HMAC-Tests benötigen mbedcrypto (pkg-config).'
fi
