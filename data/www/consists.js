import { elementIndex } from './utils.js';

let dragIndex = null;

const Modal = {
  props: {
    consist: [Boolean, Object],
  },
  data() {
    return {
      name: '',
      replicateFunctions: false,
      members: [],       // [{ address, reversed }]
      newAddress: '',
      newReversed: false,
      addingMember: false,
      originalFile: null,
      roster: {},   // address -> name
      dirtyNames: {},
      submitted: false,
    }
  },
  emits: ['close', 'update', 'del'],
  watch: {
    consist: {
      immediate: true,
      async handler() {
        const rosterResp = await fetch('/locos');
        if (rosterResp.ok) {
          const list = await rosterResp.json();
          this.roster = Object.fromEntries(
            list.map(l => [parseInt(l.file.match(/\d+/)?.[0]), l.name])
          );
        }
        if (this.consist && this.consist.file) {
          const response = await fetch(this.consist.file);
          if (response.ok) {
            const data = await response.json();
            this.name = data.name || '';
            this.replicateFunctions = data.replicateFunctions || false;
            this.members = (data.members || []).map(m => ({ ...m }));
            this.submitted = false;
            this.originalFile = this.consist.file;
          }
        } else {
          this.name = '';
          this.replicateFunctions = false;
          this.members = [];
          this.newAddress = '';
          this.newReversed = false;
          this.addingMember = false;
          this.dirtyNames = {};
          this.submitted = false;
          this.originalFile = null;
        }
      }
    }
  },
  computed: {
    leadAddress() {
      return this.members.length > 0 ? this.members[0].address : null;
    },
    newFile() {
      return this.leadAddress ? `/consists/${this.leadAddress}.json` : null;
    },
    canSave() {
      return this.name.trim() !== '' && this.members.length >= 2;
    },
    availableRoster() {
      const inConsist = new Set(this.members.map(m => m.address));
      return Object.entries(this.roster).filter(([addr]) => !inConsist.has(parseInt(addr)));
    }
  },
  methods: {
    close() {
      this.$emit('close');
    },
    addMember() {
      const addr = parseInt(this.newAddress);
      if (!addr || addr < 1 || addr > 10239) return;
      if (this.members.some(m => m.address === addr)) return;
      this.members.push({ address: addr, reversed: this.newReversed });
      this.newAddress = '';
      this.newReversed = false;
      this.addingMember = false;
    },
    removeMember(index) {
      if (index === 0 && this.members.length > 1) {
        if (!confirm('Removing the lead loco will make the next loco the new lead. Continue?')) return;
      }
      this.members.splice(index, 1);
    },
    toggleReversed(index) {
      this.members[index].reversed = !this.members[index].reversed;
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
      const newIndex = elementIndex(li) - 1;  // offset for header row
      if (newIndex !== dragIndex) {
        const moved = this.members.splice(dragIndex, 1);
        this.members.splice(newIndex, 0, ...moved);
        dragIndex = newIndex;
      }
    },
    dragEnd() { dragIndex = null; },
    async save() {
      this.submitted = true;
      if (!this.canSave) return;

      const file = this.newFile;
      const body = JSON.stringify({
        name: this.name.trim(),
        replicateFunctions: this.replicateFunctions,
        members: this.members,
      });

      const doSave = async () => {
        const response = await fetch(file, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/json' },
          body,
        });
        if (response.ok) {
          if (this.originalFile && this.originalFile !== file) {
            await fetch(this.originalFile, { method: 'DELETE' });
          }
          this.$emit('update', { file, name: this.name.trim(), memberCount: this.members.length });
          this.close();
        }
      };

      if (file === this.originalFile) {
        doSave();
      } else {
        const check = await fetch(file, { method: 'HEAD' });
        if (check.status === 404 || confirm('A consist with this lead address already exists. Overwrite?')) {
          if (this.originalFile) this.$emit('del', this.consist, false);
          doSave();
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
            <h5 class="modal-title">Consist Editor</h5>
            <button @click="close" type="button" class="btn-close"></button>
          </div>
          <div class="modal-body">

            <div class="mb-3 text-center text-muted small">Add at least two locos by typing a DCC address directly or selecting from the throttle loco roster. Drag <span style="letter-spacing:-1px">⠿</span> to reorder — the first loco <span class="badge bg-secondary" style="font-size:0.75em">L</span> is the lead. Toggle <strong>Dir</strong> to run a loco in reverse within the consist.</div>

            <div class="row mb-3">
              <div class="col">
                <div class="form-floating">
                  <input v-model="name" type="text" class="form-control" :class="{ 'is-invalid': submitted && !name.trim() }" required maxlength="30" placeholder="Consist Name" />
                  <label>Consist Name</label>
                </div>
              </div>
            </div>

            <div class="form-check form-switch mb-3">
              <input v-model="replicateFunctions" class="form-check-input" type="checkbox" id="repFnSwitch" />
              <label class="form-check-label" for="repFnSwitch">Replicate functions to all members</label>
            </div>

            <div class="mb-2 fw-semibold small text-muted">Members <span v-if="members.length" class="text-secondary">(first = lead)</span></div>
            <ul class="list-group list-group-flush mb-2" :style="submitted && members.length < 2 ? 'outline: 2px solid var(--danger)' : ''" @dragstart="dragStart" @dragover.prevent="dragOver" @dragend="dragEnd">
              <li class="list-group-item px-0 py-1">
                <div class="d-flex align-items-center gap-2 pe-2 small text-muted fw-semibold">
                  <span style="width:14px" class="flex-shrink-0"></span>
                  <span style="min-width:28px" class="flex-shrink-0">Ord</span>
                  <span style="min-width:52px" class="flex-shrink-0">No.</span>
                  <span class="flex-grow-1">Name</span>
                  <span style="width:3.5rem" class="flex-shrink-0 text-center">Dir</span>
                  <span style="width:15px" class="flex-shrink-0"></span>
                </div>
              </li>
              <li v-for="(m, i) in members" :key="m.address" class="list-group-item px-0 py-1" draggable="true">
                <div class="d-flex align-items-center gap-2 pe-2">
                  <span class="text-muted flex-shrink-0" style="width:14px; cursor:grab; font-size:1rem; line-height:1; text-align:center; display:inline-block">⠿</span>
                  <span class="badge bg-secondary flex-shrink-0" style="min-width:28px">{{ i === 0 ? 'L' : i + 1 }}</span>
                  <span class="font-monospace flex-shrink-0" style="min-width:52px">{{ m.address }}</span>
                  <span v-if="roster[m.address]" class="flex-grow-1 text-truncate small">{{ roster[m.address] }}</span>
                  <div v-else class="d-flex align-items-center flex-grow-1 gap-1">
                    <input v-model="m.name" type="text" class="form-control form-control-sm flex-grow-1 small" maxlength="30" placeholder="Unknown" @input="dirtyNames[m.address] = true" />
                    <button v-if="m.name && dirtyNames[m.address]" type="button" class="btn btn-link p-0 d-flex align-items-center text-success flex-shrink-0" title="Confirm name" tabindex="-1" @click="dirtyNames[m.address] = false">
                      <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#check"/></svg>
                    </button>
                    <span v-else style="width:16px" class="flex-shrink-0"></span>
                  </div>
                  <div class="d-flex align-items-center gap-1 flex-shrink-0" style="width:3.5rem">
                    <div class="form-check form-switch mb-0">
                      <input class="form-check-input" type="checkbox" :checked="m.reversed" @change="toggleReversed(i)" :id="'rev-' + i"
                        :style="m.reversed ? 'background-color:var(--warning);border-color:var(--warning)' : 'background-color:var(--success);border-color:var(--success)'" />
                    </div>
                    <span class="small text-muted" style="min-width:0.75rem">{{ m.reversed ? 'R' : 'F' }}</span>
                  </div>
                  <button type="button" class="btn btn-link p-0 d-flex align-items-center text-danger flex-shrink-0" @click="removeMember(i)" title="Remove">
                    <svg width="15" height="15" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
                  </button>
                </div>
              </li>
              <li v-if="addingMember" class="list-group-item px-0 py-1">
                <div class="d-flex align-items-center gap-2 pe-2">
                  <span class="text-muted flex-shrink-0" style="width:14px; font-size:1rem; line-height:1; text-align:center; display:inline-block">⠿</span>
                  <span class="badge bg-secondary flex-shrink-0" style="min-width:28px">{{ members.length === 0 ? 'L' : members.length + 1 }}</span>
                  <input v-model="newAddress" type="text" class="form-control form-control-sm font-monospace flex-shrink-0 small"
                    style="width:52px; align-self:stretch" inputmode="numeric" maxlength="5" placeholder="#"
                    @keyup.enter.prevent="addMember" />
                  <select v-model="newAddress" class="form-select form-select-sm flex-grow-1 small">
                    <option value="">— select from roster —</option>
                    <option v-for="[addr, name] in availableRoster" :key="addr" :value="addr">{{ name }}</option>
                  </select>
                  <div class="d-flex align-items-center gap-1 flex-shrink-0" style="width:3.5rem">
                    <div class="form-check form-switch mb-0">
                      <input class="form-check-input" type="checkbox" v-model="newReversed" id="new-rev"
                        :style="newReversed ? 'background-color:var(--warning);border-color:var(--warning)' : 'background-color:var(--success);border-color:var(--success)'" />
                    </div>
                    <span class="small text-muted" style="min-width:0.75rem">{{ newReversed ? 'R' : 'F' }}</span>
                  </div>
                  <span style="width:15px" class="flex-shrink-0"></span>
                </div>
              </li>
              <li v-if="addingMember" class="list-group-item px-0 py-1">
                <div class="d-flex justify-content-end gap-2 pe-2">
                  <button type="button" class="btn btn-secondary btn-sm" @click="addingMember = false; newAddress = ''; newReversed = false">Cancel</button>
                  <button type="button" class="btn btn-primary btn-sm" @click="addMember">Save Loco</button>
                </div>
              </li>
              <li class="list-group-item border-0 px-0 pt-2 pb-0">
                <button @click="addingMember = true" type="button" class="btn add-row-btn w-100 py-2">
                  <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Member
                </button>
              </li>
            </ul>
            <div v-if="members.length < 2" class="form-text text-warning">A consist needs at least 2 members to save.</div>

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
      save: false,
      consists: [],
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
    sorted() {
      return this.consists.slice().sort((a, b) =>
        (a.file.match(/\d+/)?.[0] || 0) - (b.file.match(/\d+/)?.[0] || 0)
      );
    }
  },
  methods: {
    async load() {
      this.isLoading = true;
      const response = await fetch('/consists');
      this.consists = await response.json();
      this.isLoading = false;
    },
    add() {
      this.save = {};
    },
    edit(consist) {
      this.save = consist;
    },
    async del(consist, check = true) {
      if (!check || confirm('Delete this consist?')) {
        const response = await fetch(consist.file, { method: 'DELETE' });
        if (response.ok) {
          this.consists = this.consists.filter(({ file }) => file !== consist.file);
        }
      }
    },
    update(consist) {
      const existing = this.consists.findIndex(({ file }) => file === consist.file);
      if (existing !== -1) {
        this.consists[existing] = consist;
      } else {
        this.consists.push(consist);
      }
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
      <li v-for="consist of sorted" :key="consist.file" class="list-group-item">
        <div class="row align-items-center">
          <div class="col">{{ consist.name }}</div>
          <div class="col-auto text-muted small">{{ consist.memberCount }}</div>
          <div class="col-auto d-flex flex-nowrap gap-2">
            <button @click="edit(consist)" class="btn btn-link p-0 d-flex align-items-center" title="Edit consist">
              <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#pencil"/></svg>
            </button>
            <a class="btn btn-link p-0 d-flex align-items-center" :href="consist.file" download title="Download consist config">
              <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#download"/></svg>
            </a>
            <button @click="del(consist)" type="button" class="btn btn-link p-0 d-flex align-items-center" title="Delete consist">
              <svg width="16" height="16" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
            </button>
          </div>
        </div>
      </li>
      <li v-if="!consists.length && !isLoading" class="list-group-item">
        <div class="empty-state">
          <svg width="40" height="40" fill="currentColor"><use xlink:href="bs.icons.svg#fa-train"/></svg>
          <p>No consists saved yet</p>
        </div>
      </li>
      <li class="list-group-item border-0 pt-2 pb-0">
        <button @click="add" type="button" class="btn add-row-btn w-100 py-2">
          <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align: text-bottom;"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add Consist
        </button>
      </li>
    </ul>
    <Modal v-if="save !== false" :consist="save" @close="save = false" @update="update" @del="del" />
  </div>
  `
}
