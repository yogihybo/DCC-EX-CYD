import FnEditor from "./fn.editor.js";

const Modal = {
  components: {
    FnEditor,
  },
  props: {
    loco: [Boolean, Object],
    functions: Array,
  },
  data() {
    return {
      address: '',
      name: '',
      fns: '/default.json',
      editor: [],
    }
  },
  emits: ['close', 'update', 'del'],
  watch: {
    loco: {
      immediate: true,
      async handler() {
        if (this.loco.file) {
          const response = await fetch(this.loco.file);
          if (response.ok) {
            const { name, functions } = await response.json();

            this.address = this.loco.file.match(/(\d+)/i)?.[0] || '';
            this.name = name;
            if (Array.isArray(functions)) {
              this.fns = 'custom';
              this.editor = functions;
            } else {
              this.fns = functions;
            }
          }
        }
      }
    }
  },
  computed: {
    custom() {
      return this.fns === 'custom';
    }
  },
  methods: {
    validAddress({ target }) {
      if (this.address !== '' && (this.address < 1 || this.address > 10293)) {
        target.setCustomValidity('Please enter a value between 1 & 10293.');
        target.reportValidity();
      } else {
        target.setCustomValidity('');
      }
    },
    close() {
      this.address = '';
      this.name = '';
      this.fns = '/default.json';
      this.editor = [];

      this.$emit('close');
    },
    async save() {
      const put = async file => {
        const response = await fetch(file, {
          method: 'PUT',
          headers: {
            'Content-Type': 'application/json'
          },
          body: JSON.stringify({
            name: this.name,
            functions: this.fns !== 'custom' ? this.fns : this.editor,
          })
        });
        
        if (response.ok) {
          this.$emit('update', { file, name: this.name });
          this.close();
        }
      };

      const file = `/locos/${this.address}.json`
      if (file === this.loco.file) { // no address change
        put(file);
      } else { // new loco or change of address
        const response = await fetch(file, { method: 'HEAD' });
        if (response.status === 404 || confirm('Loco with existing address, overwrite?')) {
          this.loco.file && this.$emit('del', this.loco, false);
          put(file);
        }
      }
    },
  },
  template: `
  <Teleport to="body">
    <div class="modal d-block">
      <div class="modal-dialog modal-lg modal-dialog-centered modal-dialog-scrollable">
        <form @submit.prevent="save" class="modal-content">
          <div class="modal-header">
            <h5 class="modal-title">Loco Editor</h5>
            <button @click="close" type="button" class="btn-close"></button>
          </div>
          <div class="modal-body">
            <div class="row">
              <div class="col-4 col-md-2 pe-0">
                <div class="form-floating">
                  <input v-model="address" @input="validAddress" type="text" class="form-control" required inputmode="numeric" pattern="\\d+" placeholder="#" />
                  <label>#</label>
                </div>
              </div>
              <div class="col-8 col-md-5 pe-md-0">
                <div class="form-floating">
                  <input v-model="name" type="text" class="form-control" required maxlength="20" placeholder="Loco Name" />
                  <label>Loco Name</label>
                </div>
              </div>
              <div class="col-12 col-md-5 pe-0">
                <div class="form-floating">
                  <select v-model="fns" class="form-select" required>
                    <option v-for="{ name, file } in functions" :value="file">{{ name }}</option>
                  </select>
                  <label>Loco Functions</label>
                </div>
              </div>
            </div>
            <div class="row">
              <div class="col text-center">
                <FnEditor v-model="editor" v-show="custom" class="mt-3" />
              </div>
            </div>
          </div>
          <div class="modal-footer">
            <button @click="close" type="button" class="btn btn-secondary">Close &amp; Discard</button>
            <button type="submit" class="btn btn-primary">Save Changes</button>
          </div>
        </form>
      </div>
    </div>
    <div class="modal-backdrop fade show"></div>
  </Teleport>
  `
}

export default {
  components: {
    Modal,
  },
  props: {
    active: Boolean
  },
  watch: {
    active: {
      handler(value) {
        if (value) {
          this.load();
        }
      },
      immediate: true
    }
  },
  data() {
    return {
      save: false,
      locos: [],
      isLoading: false,
      functions: [
        { file: '/default.json', name: 'Default Functions 0-28' },
        { file: 'custom', name: 'Custom Functions' }
      ],
    }
  },
  computed: {
    sorted() {
      return this.locos.sort((a, b) => (a.file.match(/(\d+)/)?.[0] || 0) - (b.file.match(/(\d+)/)?.[0] || 0));
    }
  },
  methods: {
    async load() {
      {
        this.isLoading = true;
        const response = await fetch('/locos');
        this.locos = await response.json();
        this.isLoading = false;
      }
      {
        const response = await fetch('/fns');
        this.functions = this.functions.slice(0, 2).concat(await response.json());
      }
    },
    add() {
      this.save = true;
    },
    edit(loco) {
      this.save = loco;
    },
    async del(loco, check = true) {
      if (!check || confirm('Are you sure you want to delete this loco?')) {
        const response = await fetch(loco.file, { method: 'DELETE' });
        if (response.ok) {
          this.locos = this.locos.filter(({ file }) => file !== loco.file);
        }
      }
    },
    update(loco) {
      const existing = this.locos.findIndex(({ file })=> file === loco.file);
      if (existing !== -1) { // Change existing
        this.locos[existing].name = loco.name;
      } else { // Add new
        this.locos.push(loco);
      }
    },
  },
  template: `
  <div>
    <div class="row">
      <div class="col-12">
        <ul :class="{ loading: isLoading }" class="list-group list-group-flush">
          <li class="list-group-item py-1 border-bottom">
            <div class="row small text-muted fw-semibold">
              <div class="col-3 col-md-2">#</div>
              <div class="col">Name</div>
              <div class="col-auto" style="min-width: 80px;"></div>
            </div>
          </li>
          <li v-for="loco of sorted" :key="loco.file" class="list-group-item">
            <div class="row align-items-center">
              <div class="col-3 col-md-2">#{{ loco.file.match(/\\d+/i)?.[0] }}</div>
              <div class="col">{{ loco.name }}</div>
              <div class="col-auto d-flex flex-nowrap gap-2">
                <button @click="edit(loco)" class="btn btn-link p-0 d-flex align-items-center" title="Edit loco">
                  <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#pencil"/></svg>
                </button>
                <a class="btn btn-link p-0 d-flex align-items-center" :href="loco.file" download title="Download loco config">
                  <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#download"/></svg>
                </a>
                <button @click="del(loco)" type="button" class="btn btn-link p-0 d-flex align-items-center" title="Delete loco">
                  <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
                </button>
              </div>
            </div>
          </li>
          <li v-if="!locos.length && !isLoading" class="list-group-item">
            <div class="empty-state">
              <svg width="40" height="40" fill="currentColor"><use xlink:href="bs.icons.svg#train-freight-front"/></svg>
              <p>No locos added yet</p>
            </div>
          </li>
          <li class="list-group-item border-0 pt-2 pb-0">
            <button @click="add" type="button" class="btn add-row-btn w-100 py-2">
              <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Loco
            </button>
          </li>
        </ul>
      </div>
    </div>
    <Modal v-if="save" :functions="functions" :loco="save" @close="save = false" @update="update" @del="del" />
  </div>
  `
}
