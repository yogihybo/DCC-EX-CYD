import { rand, elementIndex } from './utils.js';

const Colors = {
  'rgb(0, 0, 0)':       0x0000, // black
  'rgb(0, 0, 128)':     0x000F, // navy
  'rgb(0, 128, 0)':     0x03E0, // dark green
  'rgb(0, 128, 128)':   0x03EF, // dark cyan
  'rgb(128, 0, 0)':     0x7800, // maroon
  'rgb(128, 0, 128)':   0x780F, // purple
  'rgb(128, 128, 0)':   0x7BE0, // olive
  'rgb(211, 211, 211)': 0xD69A, // light grey
  'rgb(128, 128, 128)': 0x7BEF, // dark grey
  'rgb(0, 0, 255)':     0x001F, // blue
  'rgb(0, 255, 0)':     0x07E0, // green
  'rgb(0, 255, 255)':   0x07FF, // cyan
  'rgb(255, 0, 0)':     0xF800, // red
  'rgb(255, 0, 255)':   0xF81F, // magenta
  'rgb(255, 255, 0)':   0xFFE0, // yellow
  'rgb(255, 255, 255)': 0xFFFF, // white
  'rgb(255, 180, 0)':   0xFDA0, // orange
  'rgb(180, 255, 0)':   0xB7E0, // green yellow
  'rgb(255, 192, 203)': 0xFE19, // pink
  'rgb(150, 75, 0)':    0x9A60, // brown
  'rgb(255, 215, 0)':   0xFEA0, // gold
  'rgb(192, 192, 192)': 0xC618, // silver
  'rgb(135, 206, 235)': 0x867D, // sky blue
  'rgb(180, 46, 226)':  0x915C, // violet
};

const decToRgb = dec => Object.keys(Colors).find(key => Colors[key] === dec);

const ColorPicker = {
  data() {
    return {
      modal: false,
    }
  },
  props: ['modelValue'],
  emits: ['update:modelValue'],
  computed: {
    colors() {
      const colors = Object.keys(Colors);
      const rows = [];

      for (let i = 0; i < colors.length; i++) {
        if (i % 4 === 0) {
          rows.push([]);
        }
        rows.at(-1).push(colors[i]);
      }

      return rows;
    },
    rgb() {
      return decToRgb(this.modelValue);
    }
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

const FnIcon = {
  data() {
    return {
      icons: [],
    };
  },
  props: ['modelValue'],
  emits: ['update:modelValue'],
  methods: {
    next(index) { // Lazy load icons to not overload the ESP32
      if (this.icons[index]) {
        this.icons[index].load = true;
      }
    },
    toggle(file) {
      if (file === this.modelValue) {
        file = null;
      }
      this.$emit('update:modelValue', file);
    }
  },
  async mounted() {
    const response = await fetch('/icons');
    this.icons = (await response.json()).map((icon, idx) => {
      return {
        file: icon,
        load: idx === 0 || this.icons.findIndex(({ file }) => file === icon) !== -1
      }
    });
  },
  template: `
  <div class="fn-icons">
    <div class="d-flex flex-wrap gap-1 justify-content-center">
      <div v-for="({ file, load }, idx) of icons" :key="file"
        :class="{ 'border-primary': modelValue == file, 'border-white': modelValue != file }"
        @click="toggle(file)"
        class="border border-5 d-inline-block rounded p-1"
        role="button"
      >
        <img :src="load ? file : null" :data-src="file" @load="next(idx + 1)" />
      </div>
    </div>
  </div>
  `
};

const IconPicker = {
  components: { FnIcon },
  data() {
    return {
      modal: false,
    }
  },
  props: ['modelValue'],
  emits: ['update:modelValue'],
  methods: {
    select(icon) {
      this.modal = false;
      this.$emit('update:modelValue', icon);
    }
  },
  template: `
  <div>
    <div @click="modal = true" class="border rounded d-flex align-items-center justify-content-center" style="width: 32px; height: 32px; cursor: pointer; background: #fff;">
      <img v-if="modelValue" :src="modelValue" style="max-width: 100%; max-height: 100%;" />
      <span v-else class="text-danger">&times;</span>
    </div>
    <div v-if="modal" class="modal d-block" style="background: rgba(0,0,0,0.5); z-index: 1060;">
      <div class="modal-dialog modal-dialog-centered">
        <div class="modal-content">
          <div class="modal-header">
            <h5 class="modal-title">Select Icon</h5>
            <button @click="modal = false" type="button" class="btn-close"></button>
          </div>
          <div class="modal-body">
            <FnIcon :modelValue="modelValue" @update:modelValue="select" />
          </div>
        </div>
      </div>
    </div>
  </div>
  `
};

const FnButton = {
  components: {
    ColorPicker,
    IconPicker,
  },
  data() {
    return {
      config: {
        fn: null,
        latching: null,
        btn: {
          idle: { label: null, color: null, fill: null, border: null, icon: null },
          pressed: { label: null, color: null, fill: null, border: null, icon: null }
        }
      }
    }
  },
  props: ['modelValue'],
  emits: ['update:modelValue', 'delete'],
  watch: {
    'config.fn': function(value) {
      if (this.config.btn.idle.label.match(/F\\d*/i) && this.config.btn.pressed.label.match(/F\\d*/i)) {
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
      }
    },
    del() {
      this.$emit('delete');
    }
  },
  mounted() {
    this.load();
  },
  template: `
  <div class="card w-100 fn-row border-0 m-0">
    <div class="card-body p-2 d-flex flex-wrap align-items-center gap-3">
      
      <!-- Fn number & Latching -->
      <div class="d-flex flex-column align-items-center ms-2">
        <label class="small text-muted mb-1">Fn #</label>
        <input v-model="config.fn" type="number" class="form-control form-control-sm text-center" min="0" max="28" style="width: 60px;" />
      </div>
      <div class="d-flex flex-column align-items-center">
        <label class="small text-muted mb-1">Latching</label>
        <div class="form-check form-switch m-0 p-0 mt-1 d-flex justify-content-center">
          <input v-model="config.latching" class="form-check-input fs-5 m-0" type="checkbox" role="switch" style="margin-left: 0; float: none;">
        </div>
      </div>

      <!-- Idle & Pressed state -->
      <div class="flex-grow-1 border-start ps-3 border-end pe-3">
        <div class="d-flex align-items-center gap-2 mb-2">
          <span class="badge bg-secondary" style="width: 75px;">Idle</span>
          <input v-model="config.btn.idle.label" class="form-control form-control-sm flex-grow-1" placeholder="Label">
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Text</span>
            <ColorPicker v-model="config.btn.idle.color" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Fill</span>
            <ColorPicker v-model="config.btn.idle.fill" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Border</span>
            <ColorPicker v-model="config.btn.idle.border" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Icon</span>
            <IconPicker v-model="config.btn.idle.icon" />
          </div>
        </div>
        
        <div class="d-flex align-items-center gap-2">
          <span class="badge bg-secondary" style="width: 75px;">Pressed</span>
          <input v-model="config.btn.pressed.label" class="form-control form-control-sm flex-grow-1" placeholder="Label">
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Text</span>
            <ColorPicker v-model="config.btn.pressed.color" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Fill</span>
            <ColorPicker v-model="config.btn.pressed.fill" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Border</span>
            <ColorPicker v-model="config.btn.pressed.border" />
          </div>
          <div class="d-flex flex-column align-items-center mx-1">
            <span class="small text-muted" style="font-size: 10px; line-height: 1;">Icon</span>
            <IconPicker v-model="config.btn.pressed.icon" />
          </div>
        </div>
      </div>

      <!-- Trash -->
      <div class="ms-auto pe-2">
        <svg @click="del" class="text-danger" width="18" height="18" fill="currentColor" role="button" title="Delete Function">
          <use xlink:href="bs.icons.svg#trash"/>
        </svg>
      </div>
      
    </div>
  </div>
  `
};

let dragSibling, oldRowIndex = null;

export default {
  components: {
    FnButton,
  },
  data() {
    return {
      mutatedModel: [],
    }
  },
  props: {
    modelValue: Array,
  },
  emits: ['update:modelValue'],
  watch: {
    modelValue(value) {
      if (!value.length && this.mutatedModel.length) {
        this.mutatedModel = [];
      } else if (!this.mutatedModel.length) {
        if (value.length === 0) {
          this.mutatedModel = [ this.createFunction() ];
        } else {
          this.mutatedModel = value.map(row => {
            row.forEach(btn => {
              if (typeof btn === 'object' && !btn.key) {
                btn.key = rand();
              }
            });

            if (!Number.isInteger(row.at(-1))) {
              row.push(rand());
            }
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
            idle: { label: `F${nextFn}`, color: 65535, fill: 0, border: 65535, icon: "" },
            pressed: { label: `F${nextFn}`, color: 0, fill: 65535, border: 65535, icon: "" }
          }
        },
        rand() // row key
      ];
    },
    // Remove the keys needed by VueJS for state
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
      if (dragSibling === 'row') { // Move row
        const sibling = target.closest('.fn-row-container[draggable]');
        if (sibling) {
          const newRowIndex = elementIndex(sibling);
          if (oldRowIndex !== newRowIndex) { // Has the row moved?
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
    addRow() {
      this.mutatedModel.push(this.createFunction());
    },
    updateBtn(rowIndex, change) {
      this.mutatedModel[rowIndex][0] = change;
    },
  },
  template: `
  <div @dragstart="dragStart" @dragover="dragOver">
    <div class="mb-3 text-center text-muted small">
      Configure your functions below. Drag rows using the left handle to rearrange them.
    </div>
    <div ref="functions" class="d-inline-block position-relative pb-2 fn-grid">
      <div v-for="(row, rowIndex) of mutatedModel" :key="row.at(-1)" class="fn-row-container position-relative mt-2" draggable="true">
        <FnButton v-if="row[0]" :modelValue="row[0]"
          @update:modelValue="value => updateBtn(rowIndex, value)"
          @delete="deleteRow(rowIndex)"
        />
      </div>
    </div>
    <div class="mt-4 text-center">
      <button @click="addRow" type="button" class="btn px-4 py-2" style="border: 2px dashed #0d6efd; color: #0d6efd; background-color: transparent;">
        <svg width="20" height="20" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Function Row
      </button>
    </div>
  </div>
  `
}
