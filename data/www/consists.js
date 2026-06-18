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
      originalFile: null,
    }
  },
  emits: ['close', 'update', 'del'],
  watch: {
    consist: {
      immediate: true,
      async handler() {
        if (this.consist && this.consist.file) {
          const response = await fetch(this.consist.file);
          if (response.ok) {
            const data = await response.json();
            this.name = data.name || '';
            this.replicateFunctions = data.replicateFunctions || false;
            this.members = (data.members || []).map(m => ({ ...m }));
            this.originalFile = this.consist.file;
          }
        } else {
          this.name = '';
          this.replicateFunctions = false;
          this.members = [];
          this.newAddress = '';
          this.newReversed = false;
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
    async save() {
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
      <div class="modal-dialog modal-dialog-centered modal-dialog-scrollable">
        <form @submit.prevent="save" class="modal-content">
          <div class="modal-header">
            <h5 class="modal-title">Consist Editor</h5>
            <button @click="close" type="button" class="btn-close"></button>
          </div>
          <div class="modal-body">

            <div class="row mb-3">
              <div class="col">
                <div class="form-floating">
                  <input v-model="name" type="text" class="form-control" required maxlength="30" placeholder="Consist Name" />
                  <label>Consist Name</label>
                </div>
              </div>
            </div>

            <div class="form-check form-switch mb-3">
              <input v-model="replicateFunctions" class="form-check-input" type="checkbox" id="repFnSwitch" />
              <label class="form-check-label" for="repFnSwitch">Replicate functions to all members</label>
            </div>

            <div class="mb-2 fw-semibold small text-muted">Members <span v-if="members.length" class="text-secondary">(first = lead)</span></div>
            <ul class="list-group list-group-flush mb-3">
              <li v-for="(m, i) in members" :key="i" class="list-group-item px-0 py-1">
                <div class="d-flex align-items-center gap-2">
                  <span class="badge bg-secondary" style="min-width:28px">{{ i === 0 ? 'L' : i + 1 }}</span>
                  <span class="flex-grow-1 font-monospace">{{ m.address }}</span>
                  <button type="button"
                    class="btn btn-sm py-0 px-2"
                    :class="m.reversed ? 'btn-warning' : 'btn-outline-secondary'"
                    @click="toggleReversed(i)"
                    :title="m.reversed ? 'Reversed' : 'Normal'">
                    {{ m.reversed ? 'REV' : 'FWD' }}
                  </button>
                  <button type="button" class="btn btn-link p-0 d-flex align-items-center text-danger" @click="removeMember(i)" title="Remove">
                    <svg width="15" height="15" fill="currentColor"><use xlink:href="bs.icons.svg#trash"/></svg>
                  </button>
                </div>
              </li>
              <li v-if="!members.length" class="list-group-item px-0 text-muted small">No members yet — add at least two below.</li>
            </ul>

            <div class="row g-2 align-items-end">
              <div class="col">
                <div class="form-floating">
                  <input v-model="newAddress" type="text" class="form-control" inputmode="numeric" pattern="\\d+" maxlength="5" placeholder="Address" @keyup.enter.prevent="addMember" />
                  <label>DCC Address</label>
                </div>
              </div>
              <div class="col-auto d-flex align-items-center gap-2 pb-2">
                <div class="form-check form-switch mb-0">
                  <input v-model="newReversed" class="form-check-input" type="checkbox" id="newRevSwitch" />
                  <label class="form-check-label small" for="newRevSwitch">Rev</label>
                </div>
                <button type="button" class="btn btn-outline-primary btn-sm" @click="addMember">
                  <svg width="14" height="14" fill="currentColor"><use xlink:href="bs.icons.svg#plus-lg"/></svg> Add
                </button>
              </div>
            </div>
            <div v-if="members.length < 2" class="form-text text-warning mt-1">A consist needs at least 2 members to save.</div>

          </div>
          <div class="modal-footer">
            <button @click="close" type="button" class="btn btn-secondary">Close &amp; Discard</button>
            <button type="submit" class="btn btn-primary" :disabled="!canSave">Save Changes</button>
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
