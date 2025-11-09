"""Home Assistant integration for the Tab5 LVGL dashboard."""

from __future__ import annotations

import json
import logging
from typing import Any, Dict, List, Optional

import voluptuous as vol

from homeassistant import config_entries
from homeassistant.components import mqtt
from homeassistant.components.mqtt.models import ReceiveMessage
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall, State, callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.event import async_track_state_change_event

from .const import (
  CONF_BASE_TOPIC,
  CONF_DEVICE_ID,
  CONF_HA_PREFIX,
  CONF_SCENE_MAP,
  CONF_SENSORS,
  CONFIG_TOPIC_ROOT,
  CONFIG_TOPIC_SUB,
  DEFAULT_BASE,
  DEFAULT_PREFIX,
  DOMAIN,
  SERVICE_PUBLISH_SNAPSHOT,
)

_LOGGER = logging.getLogger(__name__)

SERVICE_SCHEMA = vol.Schema({vol.Optional("entry_id"): cv.string})


async def async_setup(hass: HomeAssistant, config: Dict[str, Any]) -> bool:
  """Set up the integration namespace and service."""
  domain_data = hass.data.setdefault(DOMAIN, {"entries": {}})

  if not hass.services.has_service(DOMAIN, SERVICE_PUBLISH_SNAPSHOT):
    async def handle_service(call: ServiceCall) -> None:
      bridge = _resolve_bridge(hass, call.data.get("entry_id"))
      if bridge is None:
          raise HomeAssistantError("No Tab5 LVGL entry configured")
      await bridge.async_publish_snapshot()

    hass.services.async_register(
      DOMAIN,
      SERVICE_PUBLISH_SNAPSHOT,
      handle_service,
      schema=SERVICE_SCHEMA,
    )

  async def _handle_bridge_config(msg: ReceiveMessage) -> None:
    try:
      payload = json.loads(msg.payload)
    except (ValueError, TypeError):
      _LOGGER.warning("Tab5 LVGL: Ungültige Bridge-Konfiguration erhalten: %s", msg.payload)
      return
    await _async_process_bridge_config(hass, payload)

  if "_config_unsub" not in domain_data:
    domain_data["_config_unsub"] = await mqtt.async_subscribe(
      hass,
      CONFIG_TOPIC_SUB,
      _handle_bridge_config,
    )

  return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
  """Create the bridge instance for a config entry."""
  bridge = Tab5Bridge(hass, entry)
  await bridge.async_setup()
  hass.data[DOMAIN]["entries"][entry.entry_id] = bridge
  entry.async_on_unload(entry.add_update_listener(_async_update_listener))
  await bridge.async_publish_config_to_device()
  return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
  """Unload a config entry."""
  bridge: Tab5Bridge = hass.data[DOMAIN]["entries"].pop(entry.entry_id)
  await bridge.async_unload()
  return True


async def _async_update_listener(hass: HomeAssistant, entry: ConfigEntry) -> None:
  """Reload when the config entry is updated."""
  await hass.config_entries.async_reload(entry.entry_id)


def _resolve_bridge(hass: HomeAssistant, entry_id: Optional[str]) -> Optional["Tab5Bridge"]:
  entries = hass.data.get(DOMAIN, {}).get("entries", {})
  if not entries:
    return None

  if entry_id:
    return entries.get(entry_id)

  # Fallback to the first (and typically only) entry
  return next(iter(entries.values()))


class Tab5Bridge:
  """Copies Home Assistant state to the Tab5 MQTT topics."""

  def __init__(self, hass: HomeAssistant, entry: ConfigEntry) -> None:
    self.hass = hass
    self.entry = entry

    data = entry.data or {}
    self.device_id = data.get(CONF_DEVICE_ID)
    self.base_topic = _normalise_topic(data.get(CONF_BASE_TOPIC, DEFAULT_BASE), DEFAULT_BASE)
    self.ha_prefix = _normalise_topic(data.get(CONF_HA_PREFIX, DEFAULT_PREFIX), DEFAULT_PREFIX)
    self.sensors: List[str] = list(data.get(CONF_SENSORS, []))
    self.scene_map: Dict[str, str] = {
      (alias or "").lower(): entity
      for alias, entity in (data.get(CONF_SCENE_MAP, {}) or {}).items()
    }
    self.config_topic = f"{CONFIG_TOPIC_ROOT}/{self.device_id}/bridge/apply" if self.device_id else None
    self._unsub_state = None
    self._unsub_connected = None
    self._unsub_scene = None
    self._unsub_request = None

  async def async_setup(self) -> None:
    """Subscribe to MQTT topics and start observers."""
    self._unsub_connected = await mqtt.async_subscribe(
      self.hass,
      f"{self.base_topic}/stat/connected",
      self._async_handle_connected,
    )
    self._unsub_scene = await mqtt.async_subscribe(
      self.hass,
      f"{self.base_topic}/cmnd/scene",
      self._async_handle_scene_command,
    )

    if self.sensors:
      self._unsub_state = async_track_state_change_event(
        self.hass,
        self.sensors,
        self._handle_state_event,
      )

    _LOGGER.info(
      "Tab5 MQTT bridge ready (device=%s, base=%s, ha_prefix=%s, sensors=%d)",
      self.device_id or "n/a",
      self.base_topic,
      self.ha_prefix,
      len(self.sensors),
    )
    if self.config_topic:
      request_topic = f"{CONFIG_TOPIC_ROOT}/{self.device_id}/bridge/request"
      self._unsub_request = await mqtt.async_subscribe(
        self.hass,
        request_topic,
        self._async_handle_request,
      )
      _LOGGER.debug("Tab5 subscribed to request topic %s", request_topic)

  async def async_unload(self) -> None:
    """Cleanup subscriptions."""
    if self._unsub_state:
      self._unsub_state()
      self._unsub_state = None
    if self._unsub_connected:
      self._unsub_connected()
      self._unsub_connected = None
    if self._unsub_scene:
      self._unsub_scene()
      self._unsub_scene = None
    if hasattr(self, "_unsub_request") and self._unsub_request:
      self._unsub_request()
      self._unsub_request = None

  async def async_publish_config_to_device(self) -> None:
    if not self.config_topic or not self.device_id:
      return
    payload = json.dumps(
      {
        "device_id": self.device_id,
        "base_topic": self.base_topic,
        "ha_prefix": self.ha_prefix,
        "sensors": self.sensors,
        "sensor_meta": self._build_sensor_meta(),
        "scene_map": self.scene_map,
      }
    )
    await mqtt.async_publish(
      self.hass,
      self.config_topic,
      payload,
      qos=1,
      retain=True,
    )

  async def async_publish_snapshot(self) -> None:
    """Push all configured sensors to MQTT."""
    for entity_id in self.sensors:
      state = self.hass.states.get(entity_id)
      if not state:
        continue
      topic = self._ha_topic_for_entity(entity_id, "state")
      payload = state.state.replace(",", ".")
      await mqtt.async_publish(self.hass, topic, payload, qos=0, retain=True)


  async def _async_handle_connected(self, msg: ReceiveMessage) -> None:
    """Handle Tab5 connection event."""
    if msg.payload == "1":
      _LOGGER.debug("Tab5 connected -> push config + snapshot")
      await self.async_publish_config_to_device()
      await self.async_publish_snapshot()

  async def _async_handle_request(self, msg: ReceiveMessage) -> None:
    """Handle explicit bridge refresh requests."""
    _LOGGER.debug("Tab5 requested bridge refresh via %s", msg.topic)
    await self.async_publish_config_to_device()

  async def _async_handle_scene_command(self, msg: ReceiveMessage) -> None:
    """Execute scene commands originating from the Tab5."""
    payload = msg.payload.strip()
    if not payload:
      return

    entity_id: Optional[str]
    if payload.startswith("scene."):
      entity_id = payload
    else:
      entity_id = self.scene_map.get(payload.lower())

    if not entity_id:
      _LOGGER.warning("Unhandled scene command from Tab5: %s", payload)
      return

    await self.hass.services.async_call(
      "scene",
      "turn_on",
      {"entity_id": entity_id},
      blocking=False,
    )

  @callback
  def _handle_state_event(self, event) -> None:
    entity_id = event.data.get("entity_id")
    new_state = event.data.get("new_state")
    if not entity_id or not new_state:
      return

    topic = self._ha_topic_for_entity(entity_id, "state")
    payload = new_state.state.replace(",", ".")
    self.hass.async_create_task(
      mqtt.async_publish(self.hass, topic, payload, qos=0, retain=True)
    )

  def _ha_topic_for_entity(self, entity_id: str, suffix: str) -> str:
    path = entity_id.replace(".", "/")
    return f"{self.ha_prefix}/{path}/{suffix}"

  def _build_sensor_meta(self) -> List[Dict[str, str]]:
    meta: List[Dict[str, str]] = []
    for entity_id in self.sensors:
      entry: Dict[str, str] = {"entity_id": entity_id}
      state: Optional[State] = self.hass.states.get(entity_id)
      if state:
        unit = state.attributes.get("unit_of_measurement")
        name = state.name
        value = state.state
        if isinstance(unit, str) and unit.strip():
          entry["unit"] = unit.strip()
        if isinstance(name, str) and name.strip():
          entry["name"] = name.strip()
        if isinstance(value, str) and value.strip():
          entry["value"] = value.strip()
      meta.append(entry)
    return meta


def _normalise_topic(value: Optional[str], default: str) -> str:
  result = (value or "").strip() or default
  while result.endswith("/"):
    result = result[:-1]
  return result or default


async def _async_process_bridge_config(hass: HomeAssistant, payload: Dict[str, Any]) -> None:
  try:
    data = _payload_to_entry_data(payload)
  except ValueError as err:
    _LOGGER.warning("Tab5 LVGL: Konfigurationspayload ignoriert (%s)", err)
    return

  device_id = data.get(CONF_DEVICE_ID)
  entry = _find_entry_by_device_id(hass, device_id)

  if entry:
    # Gerät ist bereits mit dieser Integration verbunden -> nichts zu tun
    return

  fallback = _find_entry_by_base(hass, data.get(CONF_BASE_TOPIC))
  if fallback:
    new_data = dict(fallback.data)
    changed = False
    if device_id and new_data.get(CONF_DEVICE_ID) != device_id:
      new_data[CONF_DEVICE_ID] = device_id
      changed = True
    if not changed:
      return

    _LOGGER.info("Tab5 LVGL: verknuepfe Bridge %s mit bestehender Integration", device_id)
    hass.config_entries.async_update_entry(
      fallback,
      data=new_data,
      title=_entry_title(new_data),
      unique_id=device_id or fallback.unique_id,
    )
    await hass.config_entries.async_reload(fallback.entry_id)
    return

  _LOGGER.info("Tab5 LVGL: neue Bridge entdeckt (%s) - erstelle Integration", device_id)
  hass.async_create_task(
    hass.config_entries.flow.async_init(
      DOMAIN,
      context={"source": config_entries.SOURCE_IMPORT},
      data=data,
    )
  )


def _payload_to_entry_data(payload: Dict[str, Any]) -> Dict[str, Any]:
  device_id = payload.get("device_id")
  if not device_id:
    raise ValueError("missing_device_id")

  base = _normalise_topic(payload.get("base_topic"), DEFAULT_BASE)
  prefix = _normalise_topic(payload.get("ha_prefix"), DEFAULT_PREFIX)

  sensors_raw = payload.get("sensors") or []
  if not isinstance(sensors_raw, list):
    raise ValueError("invalid_sensors")
  sensors = [str(item).strip() for item in sensors_raw if str(item).strip()]

  scene_map_raw = payload.get("scene_map") or {}
  if not isinstance(scene_map_raw, dict):
    raise ValueError("invalid_scene_map")
  scene_map: Dict[str, str] = {}
  for alias, entity in scene_map_raw.items():
    if not alias or not entity:
      continue
    scene_map[str(alias).lower()] = str(entity)

  return {
    CONF_DEVICE_ID: device_id,
    CONF_BASE_TOPIC: base,
    CONF_HA_PREFIX: prefix,
    CONF_SENSORS: sensors,
    CONF_SCENE_MAP: scene_map,
  }


def _find_entry_by_device_id(hass: HomeAssistant, device_id: Optional[str]) -> Optional[ConfigEntry]:
  if not device_id:
    return None
  for entry in hass.config_entries.async_entries(DOMAIN):
    if entry.data.get(CONF_DEVICE_ID) == device_id or entry.unique_id == device_id:
      return entry
  return None


def _find_entry_by_base(hass: HomeAssistant, base_topic: Optional[str]) -> Optional[ConfigEntry]:
  if not base_topic:
    return None
  for entry in hass.config_entries.async_entries(DOMAIN):
    if entry.data.get(CONF_BASE_TOPIC) == base_topic:
      return entry
  return None


def _entry_title(data: Dict[str, Any]) -> str:
  device_id = data.get(CONF_DEVICE_ID)
  if device_id:
    suffix = device_id[-4:].upper()
    return f"Tab5 {suffix}"
  return "Tab5 LVGL"
