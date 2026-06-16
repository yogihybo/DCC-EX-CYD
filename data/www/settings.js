export default {
  props: {
    active: Boolean
  },
  data() {
    return {
      ssid: '',
      password: '',
      server: '',
      port: 0,
      version: '',
      platform: '',
      free_ram: 0,
      ip: '',
      storageMode: 0,
      has_sd: false,
      importing: false,
    }
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
  emits: ['lock', 'unlock', 'valid'],
  methods: {
    lock() {
      this.$emit('lock');
    },
    unlock(valid = null) {
      this.$emit('unlock', valid);
    },
    async load() {
      const response = await fetch('/cs');
      if (response.ok) {
        ({
          ssid: this.ssid,
          password: this.password,
          server: this.server,
          port: this.port,
          version: this.version,
          platform: this.platform,
          free_ram: this.free_ram,
          ip: this.ip,
          storageMode: this.storageMode,
          has_sd: this.has_sd
        } = await response.json());
      }
    },
    async save() {
      const response = await fetch('/cs', {
        method: 'PUT',
        headers: {
          'Content-Type': 'application/json'
        },
        body: JSON.stringify({
          ssid: this.ssid,
          password: this.password,
          server: this.server,
          port: this.port,
        })
      });

      if (response.ok) {
        this.unlock(true);
      }
    },
    discard() {
      this.load();
      this.unlock();
    },
    exportAll() {
      const a = document.createElement('a');
      a.href = '/backup';
      a.download = 'dcc-ex-cyd-backup.json';
      a.click();
    },
    async importAll({ target }) {
      const file = target.files[0];
      if (!file) return;
      this.importing = true;
      target.value = '';
      try {
        const text = await file.text();
        const backup = JSON.parse(text);
        for (const { path, data } of backup.locos || []) {
          await fetch(path, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) });
        }
        for (const { path, data } of backup.fns || []) {
          await fetch(path, { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) });
        }
        if (backup.groups) {
          await fetch('/groups.json', { method: 'PUT', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(backup.groups) });
        }
        alert('Import complete. Reload the other tabs to see the restored configs.');
      } catch (e) {
        alert('Import failed: ' + e.message);
      } finally {
        this.importing = false;
      }
    },
  },
  template: `
  <form @submit.prevent="save" class="mt-2">

    <!-- ── Connections ─────────────────────────────────────────────────── -->
    <div class="settings-category">--- Connections ---</div>

    <div class="mb-3 row">
      <div class="col pe-0">
        <div class="form-floating">
          <input v-model="ssid" @input="lock" type="text" class="form-control" required placeholder="WiFi SSID">
          <label>WiFi SSID</label>
        </div>
      </div>
      <div class="col">
        <div class="form-floating">
          <input v-model="password" @input="lock" type="password" class="form-control" placeholder="WiFi Password" />
          <label>WiFi Password</label>
        </div>
      </div>
    </div>
    <div class="mb-3 row">
      <div class="col pe-0">
        <div class="form-floating">
          <input v-model="server" @input="lock" type="text" class="form-control" required placeholder="Command Station Host/IP" />
          <label>Command Station Host/IP</label>
        </div>
      </div>
      <div class="col">
        <div class="form-floating">
          <input v-model="port" @input="lock" type="text" class="form-control" required placeholder="Command Station Port" />
          <label>Command Station Port</label>
        </div>
      </div>
    </div>
    <div class="mb-3">
      <div class="card">
        <div class="card-body py-2 px-3">
          <p class="mb-1 small text-muted">If your CS connects to your home WiFi:</p>
          <p class="mb-2 small">Host/IP <span class="badge text-bg-secondary">dccex</span> &nbsp; Port <span class="badge text-bg-secondary">2560</span></p>
          <p class="mb-1 small text-muted">If your CS is in AP mode:</p>
          <p class="mb-0 small">Host/IP <span class="badge text-bg-secondary">192.168.4.1</span> &nbsp; Port <span class="badge text-bg-secondary">2560</span></p>
        </div>
      </div>
    </div>

    <!-- ── Storage ─────────────────────────────────────────────────────── -->
    <div class="settings-category">--- Storage ---</div>

    <div class="mb-3">
      <div class="form-floating">
        <select class="form-select" v-model="storageMode" disabled>
          <option :value="0">Internal Memory (LittleFS)</option>
          <option :value="1">SD Card</option>
        </select>
        <label>Storage Location</label>
      </div>
      <div class="form-text text-muted">Change storage location on the device under Settings &rarr; Storage.</div>
    </div>

    <div class="d-flex gap-2 mb-3">
      <button type="button" @click="exportAll" class="btn btn-secondary flex-grow-1">
        <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align:-2px"><use xlink:href="bs.icons.svg#download"/></svg>Export All
      </button>
      <button type="button" @click="$refs.importInput.click()" class="btn btn-secondary flex-grow-1" :disabled="importing">
        <svg width="16" height="16" fill="currentColor" class="me-2" style="vertical-align:-2px"><use xlink:href="bs.icons.svg#upload"/></svg>{{ importing ? 'Importing…' : 'Import All' }}
      </button>
      <input ref="importInput" type="file" accept=".json" class="d-none" @change="importAll">
    </div>

    <!-- ── Save / Discard ──────────────────────────────────────────────── -->
    <div class="text-end">
      <hr class="bg-secondary" />
      <button type="button" @click="discard" class="btn btn-secondary me-2">Discard Changes</button>
      <button type="submit" class="btn btn-primary">Save</button>
    </div>

    <!-- ── About ───────────────────────────────────────────────────────── -->
    <div class="settings-category mt-3">--- About ---</div>

    <ul class="list-group list-group-flush mb-3">
      <li class="list-group-item d-flex justify-content-between align-items-center">
        Version
        <span class="badge text-bg-secondary">{{ version || 'Unknown' }}</span>
      </li>
      <li class="list-group-item d-flex justify-content-between align-items-center">
        Platform
        <span class="badge text-bg-secondary">{{ platform || 'Unknown' }}</span>
      </li>
      <li class="list-group-item d-flex justify-content-between align-items-center">
        IP Address
        <span class="badge text-bg-secondary">{{ ip || 'Unknown' }}</span>
      </li>
      <li class="list-group-item d-flex justify-content-between align-items-center">
        Command Station
        <span class="badge text-bg-secondary">{{ server ? server + ':' + port : 'Unknown' }}</span>
      </li>
      <li class="list-group-item d-flex justify-content-between align-items-center">
        Free RAM
        <span class="badge text-bg-secondary">{{ free_ram ? free_ram + ' KB' : 'Unknown' }}</span>
      </li>
    </ul>

  </form>
  `
}
