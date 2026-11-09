#pragma once

// Persist OFF before removing credentials, so a restart cannot restore loads.
template<class Storage, class SwitchOff>
bool relayFactoryReset(Storage& storage, SwitchOff switchOff) {
  switchOff();
  if (storage.putUChar("relaymask", 0) != 1 || storage.getUChar("relaymask", 255) != 0) return false;
  if (storage.isKey("config") && !storage.remove("config")) return false;
  return !storage.isKey("config");
}
