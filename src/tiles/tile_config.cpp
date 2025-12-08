#include "src/tiles/tile_config.h"
#include <Preferences.h>

static const char* PREF_NAMESPACE = "tab5_tiles";

TileConfig tileConfig;

TileConfig::TileConfig() = default;

bool TileConfig::load() {
  bool home_ok = loadGrid("home", home_grid);
  bool game_ok = loadGrid("game", game_grid);
  return home_ok && game_ok;
}

bool TileConfig::save(const TileGridConfig& home, const TileGridConfig& game) {
  bool home_ok = saveGrid("home", home);
  bool game_ok = saveGrid("game", game);

  if (home_ok && game_ok) {
    home_grid = home;
    game_grid = game;
    Serial.println("[TileConfig] Konfiguration gespeichert");
    return true;
  }

  return false;
}

bool TileConfig::loadGrid(const char* prefix, TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, true)) {
    return false;
  }

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    char key[32];

    // Typ
    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    grid.tiles[i].type = static_cast<TileType>(prefs.getUChar(key, TILE_EMPTY));

    // Title
    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i));
    grid.tiles[i].title = prefs.getString(key, "");

    // Farbe
    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i));
    grid.tiles[i].bg_color = prefs.getUInt(key, 0);

    // Sensor-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_entity = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_unit = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i));
    grid.tiles[i].sensor_decimals = prefs.getUChar(key, 0xFF);

    // Scene-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    grid.tiles[i].scene_alias = prefs.getString(key, "");

    // Key-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_macro = prefs.getString(key, "");

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_code = prefs.getUChar(key, 0);

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    grid.tiles[i].key_modifier = prefs.getUChar(key, 0);
  }

  prefs.end();
  Serial.printf("[TileConfig] Grid '%s' geladen\n", prefix);
  return true;
}

bool TileConfig::saveGrid(const char* prefix, const TileGridConfig& grid) {
  Preferences prefs;
  if (!prefs.begin(PREF_NAMESPACE, false)) {
    return false;
  }

  for (size_t i = 0; i < TILES_PER_GRID; ++i) {
    char key[32];
    const Tile& tile = grid.tiles[i];

    // Typ
    snprintf(key, sizeof(key), "%s_t%u_type", prefix, static_cast<unsigned>(i));
    prefs.putUChar(key, static_cast<uint8_t>(tile.type));

    // Title
    snprintf(key, sizeof(key), "%s_t%u_title", prefix, static_cast<unsigned>(i));
    prefs.putString(key, tile.title);

    // Farbe
    snprintf(key, sizeof(key), "%s_t%u_color", prefix, static_cast<unsigned>(i));
    prefs.putUInt(key, tile.bg_color);

    // Sensor-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_ent", prefix, static_cast<unsigned>(i));
    prefs.putString(key, tile.sensor_entity);

    snprintf(key, sizeof(key), "%s_t%u_unit", prefix, static_cast<unsigned>(i));
    prefs.putString(key, tile.sensor_unit);

    snprintf(key, sizeof(key), "%s_t%u_prec", prefix, static_cast<unsigned>(i));
    prefs.putUChar(key, tile.sensor_decimals);

    // Scene-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_scene", prefix, static_cast<unsigned>(i));
    prefs.putString(key, tile.scene_alias);

    // Key-spezifisch
    snprintf(key, sizeof(key), "%s_t%u_macro", prefix, static_cast<unsigned>(i));
    prefs.putString(key, tile.key_macro);

    snprintf(key, sizeof(key), "%s_t%u_code", prefix, static_cast<unsigned>(i));
    prefs.putUChar(key, tile.key_code);

    snprintf(key, sizeof(key), "%s_t%u_mod", prefix, static_cast<unsigned>(i));
    prefs.putUChar(key, tile.key_modifier);
  }

  prefs.end();
  Serial.printf("[TileConfig] Grid '%s' gespeichert\n", prefix);
  return true;
}
