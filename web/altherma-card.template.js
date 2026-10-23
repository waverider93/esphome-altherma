/*GENERATED*/
// Altherma card for Home Assistant: the drawing, the side panel, the control and all readings of the ESP page
// (web/altherma.html), fed from Home Assistant entities instead of values.json.
// Derived from the ESPAltherma web interface (https://github.com/raomin/ESPAltherma), MIT License, Copyright (c) 2020 Raomin.
//
// Install: copy to /config/www/altherma-card.js, add the dashboard resource /local/altherma-card.js (JavaScript module).
//
//   type: custom:altherma-card
//   device: altherma            # ESPHome device: its X10A entities, as on the ESP page
//   control: esphome            # the controller card from the device's controller entities (optional)
//
// Without the ESP (values from P1P2MQTT, the controller from Home Assistant's shadow controller):
//
//   type: custom:altherma-card
//   values: p1p2
//   control: schattenregler_esp
//   model: EHSX04P50EF
//
// Optional: lang (en, de, fr, it, es; default: Home Assistant's), model (text or entity), connected (entity),
// values as a list of {label: <ESPAltherma label>, entity: <entity id>, name: <shown name>} (added to the device or
// preset values, replacing the same label), control as an object with the keys of the ESP's control JSON (and title). In control,
// each value is a number, true/false, an entity id, "entity_id#attribute", or {entity, mul, add}.

const STYLE = /*STYLE*/;
const MARKUP = /*MARKUP*/;
// entity name (ESPHome names entities after the X10A label in the hub language) -> English label
const NAMES = /*NAMES*/;
// English label -> {language: name}, for values from other entities
const LABELS = /*LABELS*/;

function boot(root, host) {
/*SCRIPT*/
}

const VALUE_PRESETS = {
  p1p2: [
    ['R1T-Outdoor air temp.', 'sensor.p1p2_sensors_temperature_outside'],
    ['Leaving water temp. before BUH (R1T)', 'sensor.p1p2_sensors_temperature_r1t_hp2gas_water'],
    ['Leaving water temp. after BUH (R2T)', 'sensor.p1p2_sensors_temperature_r2t_leaving_water'],
    ['Refrig. Temp. liquid side (R3T)', 'sensor.p1p2_sensors_temperature_r3t_refrigerant'],
    ['Inlet water temp.(R4T)', 'sensor.p1p2_sensors_temperature_r4t_return_water'],
    ['DHW tank temp. (R5T)', 'sensor.p1p2_dhw_temperature_r5t_dhw_tank'],
    ['Indoor ambient temp. (R1T)', 'sensor.p1p2_sensors_temperature_room'],
    ['RT setpoint', 'sensor.p1p2_room_room_heating_setpoint'],
    ['LW setpoint (main)', 'sensor.p1p2_lwt_lwt_setpoint'],
    ['DHW setpoint', 'sensor.p1p2_dhw_dhw_setpoint'],
    ['Operation Mode', 'select.p1p2_mode_heating_cooling_auto'],
    ['INV frequency (rps)', 'sensor.p1p2_mode_compressor_rpm'],
    ['INV primary current (A)', 'sensor.p1p2_power_inverter_primary_current'],
    ['3way valve(On:DHW_Off:Space)', 'binary_sensor.p1p2_dhw_valve_dhw_tank'],
    ['Water pump operation', 'binary_sensor.p1p2_mode_circulation_pump'],
    ['Flow sensor (l/min)', 'sensor.p1p2_sensors_flow'],
    ['Water pressure', 'sensor.p1p2_sensors_water_pressure'],
    ['Thermostat ON/OFF', 'binary_sensor.p1p2_mode_external_thermostat_heating'],
    ['Defrost Operation', 'binary_sensor.p1p2_mode_defrost_active'],
    ['Silent Mode', 'sensor.p1p2_quiet_quiet_mode_active'],
    ['BUH Step1', 'binary_sensor.p1p2_mode_buh'],
    ['Error Code', 'sensor.p1p2_mode_errorcode1'],
  ],
};

const CONTROL_PRESETS = {
  // the controller of the ESP (examples/altherma-ech2o.yaml), entity ids after the device name
  esphome: d => ({
    active: {entity: `select.${d}_regelung`, is: 'ESP'}, request: `switch.${d}_heizanforderung`, reason: `sensor.${d}_regler_grund`,
    deficit: `sensor.${d}_regler_defizit`, start: `number.${d}_regler_start`, stop: `number.${d}_regler_stopp`,
    min_run: `number.${d}_regler_mindestlaufzeit`, min_pause: `number.${d}_regler_mindestpause`, room_gain: `number.${d}_regler_raumkorrektur`,
    ua: `sensor.${d}_regler_ua_gelernt`, calibrations: null, calibration_left: `sensor.${d}_regler_nachste_kalibrierung`,
    compressor: 0, self_stops: `sensor.${d}_regler_eigenstopps_wp`, loss: `sensor.${d}_regler_warmeverlust`, heat: `sensor.${d}_regler_heizleistung`,
    room_low: 0.5, room_high: 1.0,
  }),
  // Home Assistant's shadow controller with the ESP's logic (homeassistant-configs, waermebilanz_schattenregler_esp.yaml)
  schattenregler_esp: () => ({
    title: {en: 'Control (simulated in Home Assistant, switches nothing)', de: 'Regelung (Simulation in Home Assistant, schaltet nichts)'},
    active: true, request: 'binary_sensor.warmebilanz_schattenregler_esp_heizanforderung', reason: 'sensor.warmebilanz_schattenregler_esp#grund',
    deficit: 'sensor.warmebilanz_schattenregler_esp', start: 'input_number.warmebilanz_startschwelle', stop: 'input_number.warmebilanz_stoppschwelle',
    min_run: 2, min_pause: 1, room_gain: 5, ua: 'sensor.warmebilanz_schattenregler_esp#ua_gelernt',
    calibrations: 'sensor.warmebilanz_schattenregler_esp#kalibrierungen',
    calibration_left: {entity: 'sensor.warmebilanz_schattenregler_esp#fenster_h', mul: -1, add: 72},
    room_low: 0.5, room_high: 1.0,
  }),
};

const BAD = /^(unavailable|unknown|none)$/i;
const slug = s => String(s || '').toLowerCase().normalize('NFKD').replace(/ß/g, 'ss').replace(/[̀-ͯ]/g, '').replace(/[^a-z0-9]+/g, '_').replace(/^_|_$/g, '');

class AlthermaCard extends HTMLElement {
  setConfig(config) {
    this._config = config || {};
    if (!this.shadowRoot) {
      const root = this.attachShadow({mode: 'open'});
      root.innerHTML = `<style>${STYLE}</style>${MARKUP}`;
      this._page = boot(root, this);
    }
    this._last = null;
    if (this._hass) this._render();
  }

  set hass(hass) {
    this._hass = hass;
    this.toggleAttribute('dark', !!(hass.themes && hass.themes.darkMode));
    this._render();
  }

  connectedCallback() { this._last = null; if (this._hass) this._render(); }
  getCardSize() { return 14; }
  getGridOptions() { return {columns: 'full'}; }
  static getStubConfig() { return {values: 'p1p2', control: 'schattenregler_esp'}; }

  _state(id) {
    const [entity, attr] = String(id).split('#');
    const s = this._hass.states[entity];
    if (!s) return undefined;
    return attr ? s.attributes[attr] : s.state;
  }

  _num(ref) {
    if (ref == null) return null;
    if (typeof ref === 'number') return ref;
    if (typeof ref === 'boolean') return ref ? 1 : 0;
    if (typeof ref === 'object') { const v = this._num(ref.entity); return v == null ? null : v * (ref.mul == null ? 1 : ref.mul) + (ref.add || 0); }
    const v = parseFloat(this._state(ref));
    return isNaN(v) ? null : v;
  }

  _bool(ref) {
    if (ref == null || typeof ref === 'boolean') return ref;
    if (typeof ref === 'object') return String(this._state(ref.entity)) === ref.is;
    const v = this._state(ref);
    return v == null ? null : /^(on|true|1|heat)$/i.test(String(v));
  }

  _device() {
    const want = this._config.device;
    if (!want || !this._hass.devices) return null;
    return Object.values(this._hass.devices).find(d => d.id === want || [d.name_by_user, d.name].some(n => n && (n === want || slug(n) === slug(want))));
  }

  _item(label, id, name) {
    const s = this._hass.states[id];
    let v = s && !BAD.test(s.state) ? String(s.state) : '';
    if (/^0x0+$/i.test(v)) v = '0';  // P1P2MQTT writes "no error" as 0x00
    const unit = s && s.attributes.unit_of_measurement;
    let d;
    try { d = s && v !== '' && this._hass.formatEntityState ? this._hass.formatEntityState(s) : undefined; } catch (e) { d = undefined; }
    return {l: label, n: name || label, v: v !== '' && unit ? v + ' ' + unit : v, d};
  }

  _values() {
    const c = this._config, hass = this._hass, list = [];
    const dev = this._device();
    let connected = true;
    if (dev) {
      const devName = dev.name_by_user || dev.name || '';
      for (const e of Object.values(hass.entities || {})) {
        if (e.device_id !== dev.id || e.hidden || !hass.states[e.entity_id] || !/^(sensor|binary_sensor)\./.test(e.entity_id)) continue;
        const friendly = hass.states[e.entity_id].attributes.friendly_name || e.entity_id;
        const name = (e.name || (friendly.startsWith(devName + ' ') ? friendly.slice(devName.length + 1) : friendly)).replace(/ \(0x[0-9A-F]{2}\)( \d+)?$/, '');
        if (/^(Regler|Controller) /i.test(name)) continue;  // the controller has its own card
        if (/^(X10A verbunden|X10A connected)$/i.test(name)) { connected = hass.states[e.entity_id].state !== 'off'; continue; }
        list.push(this._item(NAMES[name] || name, e.entity_id, name));
      }
    }
    const extra = typeof c.values === 'string' ? (VALUE_PRESETS[c.values] || []).map(([label, entity]) => ({label, entity})) : (c.values || []);
    for (const x of extra) {
      const item = this._item(x.label, x.entity, x.name || (LABELS[x.label] || {})[this._lang()] || x.label);
      const i = list.findIndex(v => v.l === x.label);
      if (i >= 0) list[i] = item; else list.push(item);
    }
    if (c.connected) connected = this._bool(c.connected) !== false;
    else if (list.length && list.every(v => v.v === '')) connected = false;
    return {list, connected};
  }

  _lang() {
    const l = this._config.lang || (this._hass.locale && this._hass.locale.language) || this._hass.language || 'en';
    return String(l).slice(0, 2).toLowerCase();
  }

  _model() {
    const m = this._config.model;
    if (m) return this._hass.states[m] ? this._state(m) : m;
    const dev = this._device();
    if (!dev) return '';
    const e = Object.values(this._hass.entities || {}).find(x => x.device_id === dev.id && /_(modell|model)$/.test(x.entity_id));
    const v = e && this._state(e.entity_id);
    return v && !BAD.test(v) ? v : (dev.model || '');
  }

  _control(values) {
    let c = this._config.control;
    if (!c) return null;
    if (typeof c === 'string') {
      const dev = this._device();
      const p = CONTROL_PRESETS[c];
      if (!p) return null;
      c = p(slug(this._config.device_prefix || (dev && (dev.name_by_user || dev.name)) || this._config.device || 'altherma'));
    }
    const n = k => this._num(c[k]);
    const out = {active: this._bool(c.active) !== false, request: !!this._bool(c.request), reason: c.reason ? String(this._state(c.reason) ?? '') : '',
      deficit: n('deficit'), start: n('start'), stop: n('stop'), min_run: n('min_run'), min_pause: n('min_pause'), room_gain: n('room_gain'),
      ua: n('ua'), calibrations: c.calibrations == null ? '—' : n('calibrations'), calibration_left: n('calibration_left'),
      compressor: c.compressor == null ? null : 0, self_stops: n('self_stops'), loss: n('loss'), heat: n('heat'),
      room_low: n('room_low'), room_high: n('room_high')};
    out.title = typeof c.title === 'object' && c.title ? c.title[this._lang()] || c.title.en : c.title;
    if (out.deficit == null || out.start == null || out.stop == null) return null;
    // heat loss and heat delivered as the ESP calculates them, when no entity gives them
    const r = this._page.reading(values), f = k => { const x = parseFloat(r[k]); return isNaN(x) ? null : x; };
    const lw = f('lw2') != null ? f('lw2') : f('lw1');
    if (out.loss == null && out.ua != null && f('room') != null && f('out') != null) out.loss = out.ua * (f('room') - f('out')) / 1000;
    if (out.heat == null && f('flow') != null && lw != null && f('inlet') != null)
      out.heat = /^(on|1|true)/i.test(r.v3 || '') ? 0 : Math.max(0, f('flow') / 60 * 4.186 * (lw - f('inlet')));
    return out;
  }

  _render() {
    if (!this._hass || !this._page || !this.isConnected) return;
    const {list, connected} = this._values();
    const data = {model: String(this._model() || '').replace(/^Altherma\((.*)\)$/, '$1'), lang: this._lang(), connected, values: list, control: this._control(list)};
    const key = JSON.stringify(data);
    if (key === this._last) return;
    this._last = key;
    this._page.update(data);
    if (data.control && data.control.title) this.shadowRoot.getElementById('cTitle').textContent = data.control.title;
  }
}

if (!customElements.get('altherma-card')) {
  customElements.define('altherma-card', AlthermaCard);
  window.customCards = window.customCards || [];
  window.customCards.push({type: 'altherma-card', name: 'Altherma', description: 'Drawing of the heat pump with its readings and the controller, as on the ESP page'});
}
