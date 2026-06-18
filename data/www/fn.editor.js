import { rand, elementIndex } from './utils.js';

const Colors = {
  'rgb(0, 0, 0)':       0x0000,
  'rgb(0, 0, 128)':     0x000F,
  'rgb(0, 128, 0)':     0x03E0,
  'rgb(0, 128, 128)':   0x03EF,
  'rgb(128, 0, 0)':     0x7800,
  'rgb(128, 0, 128)':   0x780F,
  'rgb(128, 128, 0)':   0x7BE0,
  'rgb(211, 211, 211)': 0xD69A,
  'rgb(128, 128, 128)': 0x7BEF,
  'rgb(0, 0, 255)':     0x001F,
  'rgb(0, 255, 0)':     0x07E0,
  'rgb(0, 255, 255)':   0x07FF,
  'rgb(255, 0, 0)':     0xF800,
  'rgb(255, 0, 255)':   0xF81F,
  'rgb(255, 255, 0)':   0xFFE0,
  'rgb(255, 255, 255)': 0xFFFF,
  'rgb(255, 180, 0)':   0xFDA0,
  'rgb(180, 255, 0)':   0xB7E0,
  'rgb(255, 192, 203)': 0xFE19,
  'rgb(150, 75, 0)':    0x9A60,
  'rgb(255, 215, 0)':   0xFEA0,
  'rgb(192, 192, 192)': 0xC618,
  'rgb(135, 206, 235)': 0x867D,
  'rgb(180, 46, 226)':  0x915C,
};

const decToRgb = dec => Object.keys(Colors).find(key => Colors[key] === dec);

function rgb565ToCss(c565) {
  if (c565 == null) return null;
  const r = Math.round(((c565 >> 11) & 0x1F) * 255 / 31);
  const g = Math.round(((c565 >>  5) & 0x3F) * 255 / 63);
  const b = Math.round((c565 & 0x1F) * 255 / 31);
  return `rgb(${r},${g},${b})`;
}

// cp = FA6 free solid unicode codepoint
const FA_ICONS = [
  // Lighting
  { icon: 'lightbulb',            label: 'Light',    cp: 0xf0eb },
  { icon: 'sun',                  label: 'Sun',      cp: 0xf185 },
  { icon: 'moon',                 label: 'Moon',     cp: 0xf186 },
  { icon: 'bolt',                 label: 'Bolt',     cp: 0xf0e7 },
  { icon: 'fire',                 label: 'Fire',     cp: 0xf06d },
  { icon: 'fire-flame-simple',    label: 'Flame',    cp: 0xe4f1 },
  { icon: 'eye',                  label: 'Eye',      cp: 0xf06e },
  { icon: 'eye-slash',            label: 'Eye Off',  cp: 0xf070 },
  // Sound
  { icon: 'volume-high',          label: 'Volume',   cp: 0xf028 },
  { icon: 'volume-low',           label: 'Vol Low',  cp: 0xf027 },
  { icon: 'volume-off',            label: 'Mute',     cp: 0xf026 },
  { icon: 'bell',                 label: 'Bell',     cp: 0xf0f3 },
  { icon: 'music',                label: 'Music',    cp: 0xf001 },
  { icon: 'bullhorn',             label: 'Horn',     cp: 0xf0a1 },
  // Motion / Direction
  { icon: 'rotate-left',          label: 'Rev',      cp: 0xf2ea },
  { icon: 'rotate-right',         label: 'Fwd',      cp: 0xf2f9 },
  { icon: 'arrows-rotate',        label: 'Rotate',   cp: 0xf021 },
  { icon: 'shuffle',              label: 'Shuffle',  cp: 0xf074 },
  { icon: 'arrow-up',             label: 'Up',       cp: 0xf062 },
  { icon: 'arrow-down',           label: 'Down',     cp: 0xf063 },
  { icon: 'arrow-left',           label: 'Left',     cp: 0xf060 },
  { icon: 'arrow-right',          label: 'Right',    cp: 0xf061 },
  // Mechanical
  { icon: 'gear',                 label: 'Gear',     cp: 0xf013 },
  { icon: 'gears',                label: 'Gears',    cp: 0xf085 },
  { icon: 'fan',                  label: 'Fan',      cp: 0xf863 },
  { icon: 'wrench',               label: 'Wrench',   cp: 0xf0ad },
  { icon: 'hammer',               label: 'Hammer',   cp: 0xf6e3 },
  { icon: 'screwdriver-wrench',   label: 'Tools',    cp: 0xf7d9 },
  // Power
  { icon: 'battery-full',         label: 'Battery',  cp: 0xf240 },
  { icon: 'battery-half',         label: 'Bat Half', cp: 0xf242 },
  { icon: 'battery-empty',        label: 'Bat Empty',cp: 0xf244 },
  { icon: 'plug',                 label: 'Plug',     cp: 0xf1e6 },
  { icon: 'power-off',            label: 'Power',    cp: 0xf011 },
  // Gauges
  { icon: 'gauge',                label: 'Gauge',    cp: 0xf624 },
  { icon: 'gauge-high',           label: 'Gauge Hi', cp: 0xf625 },
  { icon: 'gauge-simple',         label: 'Gauge Lo', cp: 0xe139 },
  { icon: 'temperature-high',     label: 'Temp Hi',  cp: 0xf769 },
  { icon: 'temperature-low',      label: 'Temp Lo',  cp: 0xf76b },
  // Transport
  { icon: 'train',                label: 'Train',    cp: 0xf238 },
  { icon: 'truck',                label: 'Truck',    cp: 0xf0d1 },
  { icon: 'car',                  label: 'Car',      cp: 0xf1b9 },
  { icon: 'ship',                 label: 'Ship',     cp: 0xf21a },
  { icon: 'plane',                label: 'Plane',    cp: 0xf072 },
  { icon: 'bicycle',              label: 'Bicycle',  cp: 0xf206 },
  // Status
  { icon: 'check',                label: 'Check',    cp: 0xf00c },
  { icon: 'xmark',                label: 'Cross',    cp: 0xf00d },
  { icon: 'circle-check',         label: 'OK',       cp: 0xf058 },
  { icon: 'circle-xmark',         label: 'No',       cp: 0xf057 },
  { icon: 'circle-exclamation',   label: 'Alert',    cp: 0xf06a },
  { icon: 'triangle-exclamation', label: 'Warning',  cp: 0xf071 },
  { icon: 'circle-info',          label: 'Info',     cp: 0xf05a },
  { icon: 'flag',                 label: 'Flag',     cp: 0xf024 },
  { icon: 'star',                 label: 'Star',     cp: 0xf005 },
  { icon: 'heart',                label: 'Heart',    cp: 0xf004 },
  { icon: 'lock',                 label: 'Lock',     cp: 0xf023 },
  { icon: 'unlock',               label: 'Unlock',   cp: 0xf09c },
  // Weather
  { icon: 'cloud',                label: 'Cloud',    cp: 0xf0c2 },
  { icon: 'snowflake',            label: 'Snow',     cp: 0xf2dc },
  { icon: 'wind',                 label: 'Wind',     cp: 0xf72e },
  { icon: 'droplet',              label: 'Water',    cp: 0xf043 },
  // Misc
  { icon: 'clock',                label: 'Clock',    cp: 0xf017 },
  { icon: 'stopwatch',            label: 'Timer',    cp: 0xf2f2 },
  { icon: 'key',                  label: 'Key',      cp: 0xf084 },
  { icon: 'camera',               label: 'Camera',   cp: 0xf030 },
  { icon: 'wifi',                 label: 'WiFi',     cp: 0xf1eb },
  { icon: 'signal',               label: 'Signal',   cp: 0xf012 },
  { icon: 'map-pin',              label: 'Pin',      cp: 0xf276 },
  { icon: 'compass',              label: 'Compass',  cp: 0xf14e },
  { icon: 'house',                label: 'House',    cp: 0xf015 },
  { icon: 'circle',               label: 'Circle',   cp: 0xf111 },
  { icon: 'square',               label: 'Square',   cp: 0xf0c8 },
  { icon: 'gem',                  label: 'Gem',      cp: 0xf3a5 },
];

// Reverse lookup: codepoint → entry (used by ButtonPreview)
const FA_BY_CP = new Map(FA_ICONS.map(i => [i.cp, i]));

// lv_font_conv --range value — paste into font generation command
export const FA_LV_RANGE = FA_ICONS.map(i => '0x' + i.cp.toString(16)).join(',');

// ── Color picker ─────────────────────────────────────────────────────────────

const ColorPicker = {
  data() { return { modal: false }; },
  props: ['modelValue'],
  emits: ['update:modelValue'],
  computed: {
    colors() {
      const colors = Object.keys(Colors);
      const rows = [];
      for (let i = 0; i < colors.length; i++) {
        if (i % 4 === 0) rows.push([]);
        rows.at(-1).push(colors[i]);
      }
      return rows;
    },
    rgb() { return decToRgb(this.modelValue); }
  },
  methods: {
    select(color) {
      this.modal = false;
      this.$emit('update:modelValue', Colors[color]);
    }
  },
  template: `
  <div @click="modal = true" :style="{ '--rgb': rgb }" class="color-picker"></div>
  <div v-if="modal" class="modal d-block" style="background: rgba(0,0,0,0.5); z-index: 1060;">
    <div class="modal-dialog modal-sm modal-dialog-centered">
      <div class="modal-content">
        <div class="modal-body">
          <div class="container-fluid px-2">
            <div v-for="row of colors" class="row">
              <div v-for="color of row" class="col" :style="{ 'background-color': color }" @click="select(color)" style="height: 30px; cursor: pointer;"></div>
            </div>
          </div>
        </div>
        <div class="modal-footer">
          <button @click="modal = false" type="button" class="btn btn-secondary">Cancel</button>
        </div>
      </div>
    </div>
  </div>
  `
};

// ── Button preview ────────────────────────────────────────────────────────────

const ButtonPreview = {
  props: ['state'],
  computed: {
    fillCss()   { return rgb565ToCss(this.state?.fill)   ?? '#000000'; },
    borderCss() { return rgb565ToCss(this.state?.border) ?? '#ffffff'; },
    colorCss()  { return rgb565ToCss(this.state?.color)  ?? '#ffffff'; },
    faEntry()   { return this.state?.icon ? FA_BY_CP.get(this.state.icon) : null; },
  },
  template: `
  <div class="btn-preview" :style="{ background: fillCss, borderColor: borderCss }">
    <i v-if="faEntry" :class="['fa-solid', 'fa-' + faEntry.icon]" :style="{ color: colorCss, fontSize: '14px' }"></i>
    <span v-else class="btn-preview-label" :style="{ color: colorCss }">{{ state?.label || '?' }}</span>
  </div>
  `
};

// ── FA icon picker ────────────────────────────────────────────────────────────

const FaIconPicker = {
  props: ['modelValue'],
  emits: ['update:modelValue'],
  data() { return { modal: false, search: '' }; },
  computed: {
    filtered() {
      const q = this.search.toLowerCase();
      return q ? FA_ICONS.filter(i => i.label.toLowerCase().includes(q) || i.icon.includes(q)) : FA_ICONS;
    },
    current() { return this.modelValue ? FA_BY_CP.get(this.modelValue) : null; },
  },
  methods: {
    select(item) { this.$emit('update:modelValue', item.cp); this.modal = false; this.search = ''; },
    clear()      { this.$emit('update:modelValue', null); },
  },
  template: `
  <div class="d-flex align-items-center gap-1">
    <button type="button" @click="modal = true" class="fa-icon-trigger" :title="current?.label ?? 'None'">
      <i v-if="current" :class="['fa-solid', 'fa-' + current.icon]"></i>
      <span v-else class="text-muted" style="font-size: 10px;">None</span>
    </button>
    <button v-if="modelValue" type="button" @click="clear" class="btn btn-link p-0 text-danger" style="line-height:1;">
      <svg width="10" height="10" fill="currentColor"><use xlink:href="bs.icons.svg#x-lg"/></svg>
    </button>
  </div>

  <div v-if="modal" class="modal d-block" style="background:rgba(0,0,0,0.6); z-index:1060;">
    <div class="modal-dialog modal-dialog-centered modal-dialog-scrollable">
      <div class="modal-content">
        <div class="modal-header py-2">
          <h6 class="modal-title mb-0">Select Icon</h6>
          <button @click="modal = false" type="button" class="btn-close"></button>
        </div>
        <div class="modal-body">
          <input v-model="search" class="form-control form-control-sm mb-3" placeholder="Search…" autofocus />
          <div class="fa-icon-grid">
            <div v-for="item of filtered" :key="item.icon"
              class="fa-icon-cell"
              :class="{ 'fa-icon-selected': modelValue === item.cp }"
              @click="select(item)"
              :title="item.label"
            >
              <i :class="['fa-solid', 'fa-' + item.icon]"></i>
              <span>{{ item.label }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
  `
};

// ── Function button row ───────────────────────────────────────────────────────

const FnButton = {
  components: { ColorPicker, FaIconPicker, ButtonPreview },
  data() {
    return {
      useIcon: false,
      config: {
        fn: null,
        latching: null,
        btn: {
          idle:    { label: null, color: null, fill: null, border: null, icon: null },
          pressed: { label: null, color: null, fill: null, border: null, icon: null }
        }
      }
    }
  },
  props: ['modelValue'],
  emits: ['update:modelValue', 'delete'],
  watch: {
    'config.fn': function(value) {
      if (this.config.btn.idle.label?.match(/F\d*/i) && this.config.btn.pressed.label?.match(/F\d*/i)) {
        this.config.btn.idle.label = this.config.btn.pressed.label = `F${value}`;
      }
    },
    config: {
      handler(val) {
        this.$emit('update:modelValue', JSON.parse(JSON.stringify(val)));
      },
      deep: true
    }
  },
  methods: {
    load() {
      const stringified = JSON.stringify(this.modelValue);
      if (JSON.stringify(this.config) !== stringified) {
        this.config = JSON.parse(stringified);
        this.useIcon = !!(this.config.btn.idle.icon ?? this.config.btn.pressed.icon);
      }
    },
    del() { this.$emit('delete'); },
    toggleIcon(enabled) {
      this.useIcon = enabled;
      if (!enabled) {
        this.config.btn.idle.icon    = null;
        this.config.btn.pressed.icon = null;
      }
    },
  },
  mounted() { this.load(); },
  template: `
  <div class="card w-100 fn-row border-0 m-0">
    <div class="card-body p-2">
      <div class="d-flex align-items-stretch gap-2">

        <!-- Drag handle -->
        <span class="text-muted flex-shrink-0" style="cursor:grab; font-size:1rem; line-height:1; width:14px; text-align:center; align-self:center;">⠿</span>

        <!-- Two stacked rows: Fn#+Idle / Latch+Pressed -->
        <div class="d-flex flex-column gap-1 flex-grow-1">

          <!-- Idle row -->
          <div class="d-flex align-items-center gap-2">
            <div class="d-flex align-items-center gap-1 flex-shrink-0" style="width: 100px;">
              <label class="small text-muted mb-0">Fn</label>
              <input v-model="config.fn" type="number" class="form-control form-control-sm text-center" min="0" max="28" style="width: 55px;" />
            </div>
            <span class="badge bg-secondary" style="min-width: 55px;">Idle</span>
            <input v-model="config.btn.idle.label" class="form-control form-control-sm flex-grow-1" placeholder="Label" />
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <span style="font-size: 9px;" class="text-muted">Text</span>
              <ColorPicker v-model="config.btn.idle.color" />
            </div>
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <span style="font-size: 9px;" class="text-muted">Fill</span>
              <ColorPicker v-model="config.btn.idle.fill" />
            </div>
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <span style="font-size: 9px;" class="text-muted">Border</span>
              <ColorPicker v-model="config.btn.idle.border" />
            </div>
            <FaIconPicker v-if="useIcon"
              :modelValue="config.btn.idle.icon"
              @update:modelValue="config.btn.idle.icon = $event"
            />
            <ButtonPreview :state="config.btn.idle" />
          </div>

          <!-- Pressed row -->
          <div class="d-flex align-items-center gap-2">
            <div class="d-flex align-items-center gap-1 flex-shrink-0" style="width: 100px;">
              <label class="small text-muted mb-0">Latch</label>
              <div class="form-check form-switch m-0 p-0 d-flex">
                <input v-model="config.latching" class="form-check-input fs-5 m-0" type="checkbox" role="switch" style="float: none;">
              </div>
            </div>
            <span class="badge bg-secondary" style="min-width: 55px;">Pressed</span>
            <input v-model="config.btn.pressed.label" class="form-control form-control-sm flex-grow-1" placeholder="Label" />
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <ColorPicker v-model="config.btn.pressed.color" />
            </div>
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <ColorPicker v-model="config.btn.pressed.fill" />
            </div>
            <div class="d-flex flex-column align-items-center flex-shrink-0" style="width: 34px;">
              <ColorPicker v-model="config.btn.pressed.border" />
            </div>
            <FaIconPicker v-if="useIcon"
              :modelValue="config.btn.pressed.icon"
              @update:modelValue="config.btn.pressed.icon = $event"
            />
            <ButtonPreview :state="config.btn.pressed" />
          </div>

        </div>

        <div class="vr mx-1"></div>

        <!-- Icon toggle -->
        <div class="d-flex flex-column align-items-start flex-shrink-0">
          <div class="d-flex align-items-center gap-1">
            <span style="font-size: 9px;" class="text-muted">Icon</span>
            <div class="form-check form-switch m-0 p-0 d-flex">
              <input :checked="useIcon" @change="toggleIcon($event.target.checked)" class="form-check-input m-0" type="checkbox" role="switch" style="float: none;">
            </div>
          </div>
        </div>


      </div>
    </div>
  </div>
  `
};

// ── Root editor component ─────────────────────────────────────────────────────

let dragSibling, oldRowIndex = null;

export default {
  components: { FnButton },
  data() {
    return { mutatedModel: [] }
  },
  props: { modelValue: Array },
  emits: ['update:modelValue'],
  watch: {
    modelValue(value) {
      if (!value.length && this.mutatedModel.length) {
        this.mutatedModel = [];
      } else if (!this.mutatedModel.length) {
        if (value.length === 0) {
          this.mutatedModel = [this.createFunction()];
        } else {
          this.mutatedModel = value.map(row => {
            row.forEach(btn => {
              if (typeof btn === 'object' && !btn.key) btn.key = rand();
            });
            if (!Number.isInteger(row.at(-1))) row.push(rand());
            return row;
          });
        }
      }
    },
    mutatedModel: {
      handler(value) {
        const update = this.removeKeys(JSON.parse(JSON.stringify(value)));
        if (JSON.stringify(update) !== JSON.stringify(this.modelValue)) {
          this.$emit('update:modelValue', update);
        }
      },
      deep: true,
    }
  },
  methods: {
    createFunction() {
      const nextFn = Math.min(Math.max(-1, ...this.mutatedModel.map(row => row[0]?.fn ?? -1)) + 1, 28);
      return [
        {
          key: rand(),
          fn: nextFn,
          latching: true,
          btn: {
            idle:    { label: `F${nextFn}`, color: 0xFFFF, fill: 0x0000, border: 0xFFFF, icon: '' },
            pressed: { label: `F${nextFn}`, color: 0x0000, fill: 0xFFFF, border: 0xFFFF, icon: '' }
          }
        },
        rand()
      ];
    },
    removeKeys(fns) {
      return fns.map(row => row.filter(btn => typeof btn === 'object').map(btn => {
        delete btn.key;
        return btn;
      }));
    },
    dragStart(event) {
      const existing = event.target.closest('[draggable]');
      if (existing && existing.matches('.fn-row-container')) {
        dragSibling = 'row';
        oldRowIndex = elementIndex(existing);
      }
      event.dataTransfer.effectAllowed = 'move';
      event.dataTransfer.setData('text/plain', null);
    },
    dragOver({ target }) {
      if (dragSibling === 'row') {
        const sibling = target.closest('.fn-row-container[draggable]');
        if (sibling) {
          const newRowIndex = elementIndex(sibling);
          if (oldRowIndex !== newRowIndex) {
            const move = this.mutatedModel.splice(oldRowIndex, 1);
            this.mutatedModel.splice(newRowIndex, 0, ...move);
            oldRowIndex = newRowIndex;
          }
        }
      }
    },
    deleteRow(rowIndex) {
      if (confirm('Are you sure you want to delete this function?')) {
        this.mutatedModel.splice(rowIndex, 1);
      }
    },
    addRow() { this.mutatedModel.push(this.createFunction()); },
    updateBtn(rowIndex, change) { this.mutatedModel[rowIndex][0] = change; },
  },
  template: `
  <div @dragstart="dragStart" @dragover="dragOver">
    <div class="mb-3 text-center text-muted small">
      Configure your functions below. Drag rows using the right handle to rearrange them.
    </div>
    <div ref="functions" class="d-inline-block position-relative pb-2 fn-grid">
      <div v-for="(row, rowIndex) of mutatedModel" :key="row.at(-1)" class="fn-row-container position-relative mt-2" draggable="true">
        <FnButton v-if="row[0]" :modelValue="row[0]"
          @update:modelValue="value => updateBtn(rowIndex, value)"
          @delete="deleteRow(rowIndex)"
        />
      </div>
    </div>
    <div class="mt-4">
      <button @click="addRow" type="button" class="btn add-row-btn w-100 py-2">
        <svg width="20" height="20" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Function Row
      </button>
    </div>
  </div>
  `
}
