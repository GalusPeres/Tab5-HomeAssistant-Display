# Config flow + options flow for the Tab5 LVGL integration.

from __future__ import annotations

import logging
from typing import Any, Dict

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.core import callback
from homeassistant.helpers import selector

from .const import (
  CONF_BASE_TOPIC,
  CONF_DEVICE_ID,
  CONF_HA_PREFIX,
  CONF_SCENE_ENTITIES,
  CONF_SCENE_MAP,
  CONF_SCENE_MAP_TEXT,
  CONF_SENSORS,
  DEFAULT_BASE,
  DEFAULT_PREFIX,
  DOMAIN,
)

_LOGGER = logging.getLogger(__name__)

class Tab5ConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
  """Handle the initial config flow."""

  VERSION = 1

  async def async_step_user(self, user_input: Dict[str, Any] | None = None):
    errors: Dict[str, str] = {}
    if self._async_current_entries():
      return self.async_abort(reason="single_instance_allowed")

    defaults = _entry_to_form_data(user_input or {})

    if user_input is not None:
      try:
        data = _convert_form_data(user_input)
        title = _entry_title(data)
      except ValueError as err:
        errors["base"] = err.args[0]
      else:
        return self.async_create_entry(title=title, data=data)

    return self.async_show_form(
      step_id="user",
      data_schema=_build_schema(defaults),
      errors=errors,
    )

  async def async_step_import(self, import_data: Dict[str, Any]):
    device_id = import_data.get(CONF_DEVICE_ID)
    if device_id:
      await self.async_set_unique_id(device_id)
      self._abort_if_unique_id_configured()
    title = _entry_title(import_data)
    return self.async_create_entry(title=title, data=import_data)

  @staticmethod
  @callback
  def async_get_options_flow(config_entry: config_entries.ConfigEntry):
    return Tab5OptionsFlowHandler(config_entry)


class Tab5OptionsFlowHandler(config_entries.OptionsFlow):
  """Allow editing the configuration via the UI."""

  def __init__(self, entry: config_entries.ConfigEntry) -> None:
    self.config_entry = entry

  async def async_step_init(self, user_input: Dict[str, Any] | None = None):
    return await self.async_step_user(user_input)

  async def async_step_user(self, user_input: Dict[str, Any] | None = None):
    errors: Dict[str, str] = {}
    current = dict(self.config_entry.data)
    defaults = _entry_to_form_data(current)

    if user_input is not None:
      try:
        data = _convert_form_data(user_input, current)
      except ValueError as err:
        errors["base"] = err.args[0]
      else:
        _LOGGER.warning(
          "Tab5 LVGL speichert: sensors=%s scene_map=%s",
          data.get(CONF_SENSORS),
          data.get(CONF_SCENE_MAP),
        )
        self.hass.config_entries.async_update_entry(self.config_entry, data=data)
        return self.async_create_entry(title="", data={})

    return self.async_show_form(
      step_id="user",
      data_schema=_build_schema(defaults),
      errors=errors,
    )


def _build_schema(defaults: Dict[str, Any]) -> vol.Schema:
  return vol.Schema(
    {
      vol.Required(CONF_BASE_TOPIC, default=defaults.get(CONF_BASE_TOPIC, DEFAULT_BASE)): str,
      vol.Required(CONF_HA_PREFIX, default=defaults.get(CONF_HA_PREFIX, DEFAULT_PREFIX)): str,
      vol.Optional(CONF_SENSORS, default=defaults.get(CONF_SENSORS, [])): selector.selector(
        {"entity": {"multiple": True}}
      ),
      vol.Optional(CONF_SCENE_ENTITIES, default=defaults.get(CONF_SCENE_ENTITIES, [])): selector.selector(
        {"entity": {"domain": ["scene"], "multiple": True}}
      ),
      vol.Optional(CONF_SCENE_MAP_TEXT, default=defaults.get(CONF_SCENE_MAP_TEXT, "")): selector.selector(
        {"text": {"multiline": True}}
      ),
    }
  )


def _entry_to_form_data(source: Dict[str, Any]) -> Dict[str, Any]:
  data = dict(source)
  data.setdefault(CONF_BASE_TOPIC, DEFAULT_BASE)
  data.setdefault(CONF_HA_PREFIX, DEFAULT_PREFIX)
  data.setdefault(CONF_SENSORS, [])
  data.setdefault(CONF_SCENE_ENTITIES, list((source.get(CONF_SCENE_MAP) or {}).values()))
  data.setdefault(CONF_SCENE_MAP_TEXT, source.get(CONF_SCENE_MAP_TEXT, ""))
  return data


def _convert_form_data(user_input: Dict[str, Any], current: Dict[str, Any] | None = None) -> Dict[str, Any]:
  current = current or {}
  _LOGGER.debug("Tab5 options raw form data: %s", user_input)

  base = _normalise_topic(
    user_input.get(CONF_BASE_TOPIC, current.get(CONF_BASE_TOPIC, DEFAULT_BASE)),
    DEFAULT_BASE,
  )
  prefix = _normalise_topic(
    user_input.get(CONF_HA_PREFIX, current.get(CONF_HA_PREFIX, DEFAULT_PREFIX)),
    DEFAULT_PREFIX,
  )

  raw_sensors = user_input.get(CONF_SENSORS)
  if raw_sensors is None:
    raw_sensors = current.get(CONF_SENSORS, [])
  sensors = _normalise_entity_list(raw_sensors)

  scene_map = {}

  selected_scenes = user_input.get(CONF_SCENE_ENTITIES)
  if selected_scenes is None:
    selected_scenes = list((current.get(CONF_SCENE_MAP) or {}).values())
  else:
    selected_scenes = _normalise_entity_list(selected_scenes)

  for entity_id in selected_scenes:
    entity_id = (entity_id or "").strip()
    if not entity_id:
      continue
    alias = entity_id.split(".", 1)[-1]
    alias = alias.replace("scene.", "").lower()
    base_alias = alias
    idx = 2
    while alias in scene_map:
      alias = f"{base_alias}{idx}"
      idx += 1
    scene_map[alias] = entity_id

  scene_map_text = user_input.get(CONF_SCENE_MAP_TEXT, current.get(CONF_SCENE_MAP_TEXT, ""))
  scene_map_text = scene_map_text.strip("\n")
  manual_map = _parse_scene_map(scene_map_text)
  scene_map.update(manual_map)

  updated = dict(current)
  updated[CONF_BASE_TOPIC] = base
  updated[CONF_HA_PREFIX] = prefix
  updated[CONF_SENSORS] = sensors
  updated[CONF_SCENE_MAP] = scene_map
  updated[CONF_SCENE_MAP_TEXT] = scene_map_text
  return updated


def _parse_scene_map(text: str) -> Dict[str, str]:
  mapping: Dict[str, str] = {}
  for raw_line in text.splitlines():
    line = raw_line.strip()
    if not line:
      continue
    if "=" not in line:
      raise ValueError("invalid_scene_map")
    alias, entity = line.split("=", 1)
    alias = alias.strip().lower()
    entity = entity.strip()
    if not alias or not entity:
      raise ValueError("invalid_scene_map")
    mapping[alias] = entity
  return mapping


def _normalise_topic(value: str | None, default: str) -> str:
  result = (value or "").strip() or default
  while result.endswith("/"):
    result = result[:-1]
  return result or default


def _entry_title(data: Dict[str, Any]) -> str:
  device_id = data.get(CONF_DEVICE_ID)
  if device_id:
    suffix = device_id[-4:].upper()
    return f"Tab5 {suffix}"
  return "Tab5 LVGL"


def _normalise_entity_list(values: Any) -> list[str]:
  entities = []
  if not isinstance(values, list):
    values = [values] if values is not None else []
  for item in values:
    if isinstance(item, str):
      entity_id = item.strip()
    elif isinstance(item, dict):
      entity_id = str(item.get("entity_id", "")).strip()
    else:
      entity_id = str(item or "").strip()
    if entity_id:
      entities.append(entity_id)
  return entities
