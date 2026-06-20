import { rand, elementIndex } from './utils.js';
import { FA_ICONS, FA_BY_CP, FA_LV_RANGE } from './fa-icons.js';

const Colors = {
  // Neutrals
  'rgb(0, 0, 0)':       0x0000,
  'rgb(32, 32, 32)':    0x2104,
  'rgb(64, 64, 64)':    0x4208,
  'rgb(96, 96, 96)':    0x630C,
  'rgb(128, 128, 128)': 0x8410,
  'rgb(160, 160, 160)': 0xA514,
  'rgb(192, 192, 192)': 0xC618,
  'rgb(255, 255, 255)': 0xFFFF,
  // Reds
  'rgb(64, 0, 0)':      0x4000,
  'rgb(128, 0, 0)':     0x7800,
  'rgb(200, 0, 0)':     0xC000,
  'rgb(255, 0, 0)':     0xF800,
  'rgb(255, 69, 0)':    0xFA20,
  'rgb(220, 20, 60)':   0xD8A7,
  'rgb(255, 105, 180)': 0xFB56,
  'rgb(255, 192, 203)': 0xFE19,
  // Oranges & yellows
  'rgb(128, 64, 0)':    0x7900,
  'rgb(200, 100, 0)':   0xC980,
  'rgb(255, 128, 0)':   0xFC00,
  'rgb(255, 180, 0)':   0xFDA0,
  'rgb(255, 215, 0)':   0xFEA0,
  'rgb(255, 255, 0)':   0xFFE0,
  'rgb(180, 255, 0)':   0xB7E0,
  'rgb(128, 255, 0)':   0x87E0,
  // Greens
  'rgb(0, 64, 0)':      0x0200,
  'rgb(0, 128, 0)':     0x03E0,
  'rgb(34, 139, 34)':   0x2444,
  'rgb(0, 200, 0)':     0x0640,
  'rgb(0, 255, 0)':     0x07E0,
  'rgb(144, 238, 144)': 0x9772,
  'rgb(0, 255, 127)':   0x07EF,
  'rgb(0, 128, 128)':   0x03EF,
  // Blues & cyans
  'rgb(0, 255, 255)':   0x07FF,
  'rgb(135, 206, 235)': 0x867D,
  'rgb(173, 216, 230)': 0xAEBC,
  'rgb(70, 130, 180)':  0x4BD6,
  'rgb(30, 144, 255)':  0x249F,
  'rgb(0, 0, 255)':     0x001F,
  'rgb(0, 0, 192)':     0x0017,
  'rgb(0, 0, 128)':     0x000F,
  // Purples & magentas
  'rgb(75, 0, 130)':    0x4810,
  'rgb(128, 0, 128)':   0x780F,
  'rgb(147, 112, 219)': 0x939B,
  'rgb(180, 46, 226)':  0x915C,
  'rgb(238, 130, 238)': 0xEC1D,
  'rgb(255, 0, 255)':   0xF81F,
  'rgb(255, 20, 147)':  0xF891,
  'rgb(255, 182, 255)': 0xFD9F,
  // Browns & warm
  'rgb(101, 67, 33)':   0x6224,
  'rgb(150, 75, 0)':    0x9A60,
  'rgb(210, 105, 30)':  0xD344,
  'rgb(210, 180, 140)': 0xD591,
  'rgb(255, 218, 185)': 0xFED6,
  'rgb(255, 228, 196)': 0xFED7,
  'rgb(240, 230, 140)': 0xEF11,
  'rgb(211, 211, 211)': 0xD69A,
};

const decToRgb = dec => Object.keys(Colors).find(key => Colors[key] === dec);

function rgb565ToCss(c565) {
  if (c565 == null) return null;
  const r = Math.round(((c565 >> 11) & 0x1F) * 255 / 31);
  const g = Math.round(((c565 >>  5) & 0x3F) * 255 / 63);
  const b = Math.round((c565 & 0x1F) * 255 / 31);
  return `rgb(${r},${g},${b})`;
}

export { FA_ICONS, FA_BY_CP, FA_LV_RANGE } from './fa-icons.js';

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
        if (i % 8 === 0) rows.push([]);
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
      previewPressed: false,
      config: {
        fn: null,
        name: '',
        latching: null,
        idleDisplay: 'label',
        pressedDisplay: 'label',
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
      if (this.config.btn.idle.label?.match(/^F\d*$/i))    this.config.btn.idle.label    = `F${value}`;
      if (this.config.btn.pressed.label?.match(/^F\d*$/i)) this.config.btn.pressed.label = `F${value}`;
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
        if (!this.config.name)           this.config.name           = '';
        if (!this.config.idleDisplay)    this.config.idleDisplay    = this.config.btn.idle.icon    ? 'icon' : 'label';
        if (!this.config.pressedDisplay) this.config.pressedDisplay = this.config.btn.pressed.icon ? 'icon' : 'label';
      }
    },
    del() { this.$emit('delete'); },
    setIdleIcon(val) {
      this.config.btn.idle.icon = val;
      if (!val) this.config.idleDisplay = 'label';
    },
    setPressedIcon(val) {
      this.config.btn.pressed.icon = val;
      if (!val) this.config.pressedDisplay = 'label';
    },
  },
  mounted() { this.load(); },
  template: `
  <div class="fn-row d-flex">

    <!-- Left: drag grip + fn number + latch toggle -->
    <div class="fn-left d-flex align-items-center flex-shrink-0">
      <span class="fn-drag-grip">⠿</span>
      <div class="fn-num-col">
        <span class="fn-num-text">F{{ config.fn }}</span>
        <span class="fn-latch-cap">Latch</span>
        <div class="form-check form-switch m-0 p-0 d-flex justify-content-center">
          <input v-model="config.latching" class="form-check-input m-0" type="checkbox" role="switch" style="float:none; width:2em; height:1em;">
        </div>
      </div>
    </div>

    <!-- Centre: name header + idle/pressed columns -->
    <div class="fn-centre">

      <!-- Name header -->
      <div class="fn-hdr">
        <div class="fn-name-wrap">
          <span class="fn-field-label">Name</span>
          <input v-model="config.name" class="fn-name-input" placeholder="Function name">
        </div>
      </div>

      <!-- State columns -->
      <div class="fn-body-cols">

        <!-- Idle column -->
        <div class="fn-scol">
          <div class="fn-col-head-row">
            <span class="fn-col-head fn-col-idle">Idle</span>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Type</span>
            <div class="fn-pill">
              <button type="button" :class="['fn-pill-opt', config.idleDisplay === 'label' && 'fn-pill-active']" @click="config.idleDisplay = 'label'">Label</button>
              <button type="button" :class="['fn-pill-opt', config.idleDisplay === 'icon'  && 'fn-pill-active']" @click="config.idleDisplay = 'icon'">Icon</button>
            </div>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Display</span>
            <input v-show="config.idleDisplay === 'label'" v-model="config.btn.idle.label" class="fn-label-inp" placeholder="Label">
            <div v-show="config.idleDisplay === 'icon'">
              <FaIconPicker :modelValue="config.btn.idle.icon" @update:modelValue="setIdleIcon($event)" />
            </div>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Colour</span>
            <span class="fn-clbl">Text/Icon</span><ColorPicker v-model="config.btn.idle.color" />
            <span class="fn-clbl">Fill</span><ColorPicker v-model="config.btn.idle.fill" />
          </div>
        </div>

        <!-- Pressed column -->
        <div class="fn-scol fn-scol-pressed">
          <div class="fn-col-head-row">
            <span class="fn-col-head fn-col-pressed">Pressed</span>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Type</span>
            <div class="fn-pill">
              <button type="button" :class="['fn-pill-opt', config.pressedDisplay === 'label' && 'fn-pill-active']" @click="config.pressedDisplay = 'label'">Label</button>
              <button type="button" :class="['fn-pill-opt', config.pressedDisplay === 'icon'  && 'fn-pill-active']" @click="config.pressedDisplay = 'icon'">Icon</button>
            </div>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Display</span>
            <input v-show="config.pressedDisplay === 'label'" v-model="config.btn.pressed.label" class="fn-label-inp" placeholder="Label">
            <div v-show="config.pressedDisplay === 'icon'">
              <FaIconPicker :modelValue="config.btn.pressed.icon" @update:modelValue="setPressedIcon($event)" />
            </div>
          </div>
          <div class="fn-pr">
            <span class="fn-pl">Colour</span>
            <span class="fn-clbl">Text/Icon</span><ColorPicker v-model="config.btn.pressed.color" />
            <span class="fn-clbl">Fill</span><ColorPicker v-model="config.btn.pressed.fill" />
          </div>
        </div>

      </div>
    </div>

    <!-- Preview strip -->
    <div class="fn-preview-strip d-flex flex-column align-items-center justify-content-center flex-shrink-0 gap-1"
      @click="previewPressed = !previewPressed">
      <span class="fn-preview-lbl">Button Preview</span>
      <span class="fn-preview-state">{{ previewPressed ? 'Pressed' : 'Idle' }}</span>
      <div class="fn-btn-prev">
        <ButtonPreview :state="previewPressed ? config.btn.pressed : config.btn.idle" />
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
          name: '',
          latching: true,
          idleDisplay: 'label',
          pressedDisplay: 'label',
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
