#include "ArchiveHistory.h"
#include <cJSON.h>
#include <cmath>
#include <cstring>

bool ArchiveHistory::parse(const char* text, uint32_t now) {
  if (!text || now < 21600) return false;
  cJSON* root = cJSON_ParseWithOpts(text, nullptr, 1);
  if (!root) return false;
  ArchiveHistory parsed;
  bool ok = cJSON_IsObject(root);
  const cJSON* intervalNode = cJSON_GetObjectItemCaseSensitive(root, "interval_seconds");
  const cJSON* series = cJSON_GetObjectItemCaseSensitive(root, "series");
  ok = ok && cJSON_IsNumber(intervalNode) && intervalNode->valuedouble >= 60 &&
       intervalNode->valuedouble <= 3600 && floor(intervalNode->valuedouble) == intervalNode->valuedouble &&
       cJSON_IsArray(series);
  if (ok) parsed.interval = uint32_t(intervalNode->valuedouble);
  const cJSON* item = nullptr;
  cJSON_ArrayForEach(item, series) {
    if (!ok) break;
    const cJSON* id = cJSON_GetObjectItemCaseSensitive(item, "id");
    const cJSON* points = cJSON_GetObjectItemCaseSensitive(item, "points");
    if (!cJSON_IsObject(item) || !cJSON_IsNumber(id) || !std::isfinite(id->valuedouble) ||
        floor(id->valuedouble) != id->valuedouble || id->valuedouble < 0) { ok = false; break; }
    if (id->valuedouble >= SENSOR_COUNT) continue;
    Series& target = parsed.sensors[unsigned(id->valuedouble)];
    if (target.present || !cJSON_IsArray(points)) { ok = false; break; }
    target.present = true;
    uint32_t previous = 0;
    const cJSON* point = nullptr;
    cJSON_ArrayForEach(point, points) {
      const cJSON* x = cJSON_GetObjectItemCaseSensitive(point, "x");
      const cJSON* y = cJSON_GetObjectItemCaseSensitive(point, "y");
      if (!cJSON_IsObject(point) || !cJSON_IsNumber(x) || !std::isfinite(x->valuedouble) ||
          x->valuedouble < 0 || x->valuedouble > double(UINT32_MAX) * 1000.0 ||
          floor(x->valuedouble / 1000.0) != x->valuedouble / 1000.0 ||
          (!cJSON_IsNull(y) && !cJSON_IsNumber(y))) { ok = false; break; }
      const uint32_t timestamp = uint32_t(x->valuedouble / 1000.0);
      if (previous && timestamp <= previous) { ok = false; break; }
      previous = timestamp;
      if (cJSON_IsNumber(y) && (!std::isfinite(y->valuedouble) || y->valuedouble < -200 || y->valuedouble > 850)) { ok = false; break; }
      if (timestamp < now - 21600 || timestamp > now) continue;
      if (target.size == MAX_POINTS) { ok = false; break; }
      target.points[target.size++] = {timestamp, cJSON_IsNull(y) ? MISSING : int16_t(lround(y->valuedouble * 10.0))};
    }
  }
  cJSON_Delete(root);
  if (ok) *this = parsed;
  return ok;
}
