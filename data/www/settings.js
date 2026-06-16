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
      migrating: false,
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
          storageMode: this.storageMode,
        })
      });

      if (response.ok) {
        this.unlock(true);
      }
    },
    async migrateConfigs(toMode) {
      const dir = toMode === 1 ? 'SD Card' : 'Internal Memory';
      if (!confirm(`Are you sure you want to migrate your JSON configs to ${dir}? The existing files will be backed up.`)) return;
      
      this.migrating = true;
      try {
        const response = await fetch('/migrate', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ to: toMode })
        });
        
        if (response.ok) {
          alert('Configs successfully migrated!');
          this.load();
        } else {
          alert('Failed to migrate configs.');
        }
      } catch (err) {
        alert('An error occurred during migration.');
      } finally {
        this.migrating = false;
        this.unlock(true);
      }
    },
    discard() {
      this.load();
      this.unlock();
    },
  },
  template: `
  <form @submit.prevent="save" class="mt-2">
    <div class="row">
      <div class="col-12">
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
        <div class="mb-2 row">
          <div class="col pe-0">
            <div class="form-floating">
              <input v-model="server" @input="lock" type="text" class="form-control" required placeholder="Command Station Server Host/IP" />
              <label>Command Station Server Host/IP</label>
            </div>
          </div>
          <div class="col">
            <div class="form-floating">
              <input v-model="port" @input="lock" type="text" class="form-control" required placeholder="Command Station Server Port" />
              <label>Command Station Server Port</label>
            </div>
          </div>
        </div>
        <div class="mb-2 row">
          <div class="col-12 pe-0">
            <div class="form-floating">
              <select class="form-select" v-model="storageMode" @change="lock" :disabled="!has_sd">
                <option :value="0">Internal Memory (LittleFS)</option>
                <option :value="1" :disabled="!has_sd">SD Card</option>
              </select>
              <label>Active Storage Location</label>
            </div>
            <div v-if="!has_sd" class="form-text text-danger">SD Card not detected</div>
          </div>
        </div>
        <div class="mb-2 row">
          <div class="col-6 pe-1">
             <button type="button" @click="migrateConfigs(1)" class="btn btn-warning w-100" :disabled="migrating || !has_sd">
                <span v-if="migrating" class="spinner-border spinner-border-sm me-2" role="status" aria-hidden="true"></span>
                Migrate to SD
             </button>
          </div>
          <div class="col-6 ps-1">
             <button type="button" @click="migrateConfigs(0)" class="btn btn-warning w-100" :disabled="migrating || !has_sd">
                <span v-if="migrating" class="spinner-border spinner-border-sm me-2" role="status" aria-hidden="true"></span>
                Migrate to Internal
             </button>
          </div>
        </div>
      </div>
    </div>
    <div class="row">
      <div class="col-12 text-end">
        <hr class="mt-2 bg-secondary" />
        <button @click="discard" class="btn btn-secondary me-2">Discard Changes</button>
        <button type="submit" class="btn btn-primary">Save</button>
      </div>
    </div>
    <div class="row mt-3">
      <div class="col-12">
        <div class="card">
          <div class="card-header">
            <svg width="16" height="16" fill="currentColor">
              <use xlink:href="bs.icons.svg#info"/>
            </svg>
            Note
          </div>
          <div class="card-body">
            <ul class="list-group list-group-flush">
              <li class="list-group-item">If your Command Station connects to your home WiFi the defaults for Server Host/IP &amp; Port are
                <span class="badge text-bg-secondary">dccex</span> and <span class="badge text-bg-secondary">2560</span></li>
              <li class="list-group-item">If your Command Station is in AP mode the defaults for Server Host/IP &amp; Port are
                <span class="badge text-bg-secondary">192.168.4.1</span> and <span class="badge text-bg-secondary">2560</span></li>
            </ul>
          </div>
        </div>
      </div>
    </div>
    <div class="row mt-3">
      <div class="col-12">
        <div class="card">
          <div class="card-header">
            <svg width="16" height="16" fill="currentColor">
              <use xlink:href="bs.icons.svg#info"/>
            </svg>
            About DCC-EX-CYD
          </div>
          <div class="card-body">
            <ul class="list-group list-group-flush">
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
                Command Station Connection
                <span class="badge text-bg-secondary">{{ server ? server + ':' + port : 'Unknown' }}</span>
              </li>
              <li class="list-group-item d-flex justify-content-between align-items-center">
                Free RAM
                <span class="badge text-bg-secondary">{{ free_ram ? free_ram + ' KB' : 'Unknown' }}</span>
              </li>
            </ul>
          </div>
        </div>
      </div>
    </div>
  </form>
  `
}
