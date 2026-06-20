import { elementIndex } from './utils.js';

let dragIndex = null;

const Modal = {
  props: {
    group: [Boolean, Object],
    roster: Object,  // address (int) -> name
  },
  data() {
    return {
      name: '',
      members: [],   // [address (int)]
      submitted: false,
    }
  },
  emits: ['close', 'save'],
  watch: {
    group: {
      immediate: true,
      handler() {
        if (this.group && this.group !== true) {
          this.name = this.group.name || '';
          this.members = [...(this.group.locos || [])];
        } else {
          this.name = '';
          this.members = [];
        }
        this.submitted = false;
      }
    }
  },
  computed: {
    canSave() {
      return this.name.trim() !== '';
    },
    availableRoster() {
      const inGroup = new Set(this.members);
      return Object.entries(this.roster).filter(([addr]) => !inGroup.has(parseInt(addr)));
    }
  },
  methods: {
    close() { this.$emit('close'); },
    addLoco(addr) {
      const a = parseInt(addr);
      if (!a || this.members.includes(a)) return;
      this.members.push(a);
    },
    removeLoco(index) {
      this.members.splice(index, 1);
    },
    dragStart(event) {
      const li = event.target.closest('li[draggable]');
      if (!li) return;
      dragIndex = elementIndex(li) - 1;  // offset for header row
      event.dataTransfer.effectAllowed = 'move';
      event.dataTransfer.setData('text/plain', null);
    },
    dragOver(event) {
      if (dragIndex === null) return;
      const li = event.target.closest('li[draggable]');
      if (!li) return;
      const newIndex = elementIndex(li) - 1;
      if (newIndex !== dragIndex) {
        const moved = this.members.splice(dragIndex, 1);
        this.members.splice(newIndex, 0, ...moved);
        dragIndex = newIndex;
      }
    },
    dragEnd() { dragIndex = null; },
    submit() {
      this.submitted = true;
      if (!this.canSave) return;
      this.$emit('save', { name: this.name.trim(), locos: [...this.members] });
    }
  },
  template: `
  <Teleport to="body">
    <div class="modal d-block">
      <div class="modal-dialog modal-lg modal-dialog-centered modal-dialog-scrollable">
        <form @submit.prevent="submit" class="modal-content">
          <div class="modal-header">
            <h5 class="modal-title">Group Editor</h5>
            <button @click="close" type="button" class="btn-close"></button>
          </div>
          <div class="modal-body">

            <div class="row mb-3">
              <div class="col">
                <div class="form-floating">
                  <input v-model="name" type="text" class="form-control" :class="{ 'is-invalid': submitted && !name.trim() }" required maxlength="20" placeholder="Group Name" />
                  <label>Group Name</label>
                </div>
              </div>
            </div>

            <div class="mb-2 fw-semibold small text-muted">Locos</div>
            <ul class="list-group list-group-flush mb-2" @dragstart="dragStart" @dragover.prevent="dragOver" @dragend="dragEnd">
              <li class="list-group-item px-0 py-1">
                <div class="d-flex align-items-center gap-2 pe-2 small text-muted fw-semibold">
                  <span style="width:14px" class="flex-shrink-0"></span>
                  <span style="min-width:52px" class="flex-shrink-0">No.</span>
                  <span class="flex-grow-1">Name</span>
                  <span style="width:15px" class="flex-shrink-0"></span>
                </div>
              </li>
              <li v-for="(addr, i) in members" :key="addr" class="list-group-item px-0 py-1" draggable="true">
                <div class="d-flex align-items-center gap-2 pe-2">
                  <span class="text-muted flex-shrink-0" style="width:14px; cursor:grab; font-size:1rem; line-height:1; text-align:center; display:inline-block">⠿</span>
                  <span class="font-monospace flex-shrink-0" style="min-width:52px">{{ addr }}</span>
                  <span class="flex-grow-1 text-truncate small">{{ roster[addr] || 'Unknown' }}</span>
                  <button type="button" class="btn btn-link p-0 d-flex align-items-center text-danger flex-shrink-0" @click="removeLoco(i)" title="Remove">
                    <svg width="15" height="15" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
                  </button>
                </div>
              </li>
              <li class="list-group-item border-0 px-0 pt-2 pb-0">
                <select v-if="availableRoster.length" class="form-select form-select-sm" @change="addLoco($event.target.value); $event.target.value = ''">
                  <option value="">— add loco from roster —</option>
                  <option v-for="[addr, locoName] in availableRoster" :key="addr" :value="addr">{{ locoName }} (#{{ addr }})</option>
                </select>
                <p v-else-if="!members.length" class="text-muted small mb-0">No roster locos available to add.</p>
              </li>
            </ul>

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
  components: { Modal },
  props: {
    active: Boolean,
  },
  data() {
    return {
      editing: false,   // false | 'new' | index
      groups: [],
      roster: {},       // address (int) -> name
      isLoading: false,
    }
  },
  watch: {
    active: {
      handler(value) { if (value) this.load(); },
      immediate: true,
    }
  },
  computed: {
    editingGroup() {
      if (this.editing === false) return false;
      if (this.editing === 'new') return true;
      return this.groups[this.editing];
    }
  },
  methods: {
    async load() {
      this.isLoading = true;
      const [locosResp, groupsResp] = await Promise.all([fetch('/locos'), fetch('/groups.json')]);
      const locoList = await locosResp.json();
      this.roster = Object.fromEntries(
        locoList.map(l => [parseInt(l.file.match(/\d+/)?.[0]), l.name])
      );
      this.groups = groupsResp.ok ? await groupsResp.json() : [];
      this.isLoading = false;
    },
    add() { this.editing = 'new'; },
    edit(index) { this.editing = index; },
    async del(index) {
      if (!confirm('Delete this group?')) return;
      this.groups.splice(index, 1);
      await this.persist();
    },
    async saveGroup({ name, locos }) {
      if (this.editing === 'new') {
        this.groups.push({ name, locos });
      } else {
        this.groups[this.editing] = { name, locos };
      }
      this.editing = false;
      await this.persist();
    },
    async persist() {
      await fetch('/groups.json', {
        method: 'PUT',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(this.groups),
      });
    },
  },
  template: `
  <div>
    <ul :class="{ loading: isLoading }" class="list-group list-group-flush">
      <li class="list-group-item py-1 border-bottom">
        <div class="row small text-muted fw-semibold">
          <div class="col">Name</div>
          <div class="col-auto">Locos</div>
          <div class="col-auto" style="min-width: 80px;"></div>
        </div>
      </li>
      <li v-for="(group, i) in groups" :key="i" class="list-group-item">
        <div class="row align-items-center">
          <div class="col">{{ group.name }}</div>
          <div class="col-auto text-muted small">{{ group.locos.length }}</div>
          <div class="col-auto d-flex flex-nowrap gap-2">
            <button @click="edit(i)" class="btn btn-link p-0 d-flex align-items-center" title="Edit group">
              <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#pencil"/></svg>
            </button>
            <button @click="del(i)" type="button" class="btn btn-link p-0 d-flex align-items-center text-danger" title="Delete group">
              <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
            </button>
          </div>
        </div>
      </li>
      <li v-if="!groups.length && !isLoading" class="list-group-item">
        <div class="empty-state">
          <svg width="40" height="40" fill="currentColor"><use xlink:href="bs.icons.svg#collection"/></svg>
          <p>No groups added yet</p>
        </div>
      </li>
      <li class="list-group-item border-0 pt-2 pb-0">
        <button @click="add" type="button" class="btn add-row-btn w-100 py-2">
          <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Group
        </button>
      </li>
    </ul>
    <Modal v-if="editing !== false" :group="editingGroup" :roster="roster" @close="editing = false" @save="saveGroup" />
  </div>
  `
}
