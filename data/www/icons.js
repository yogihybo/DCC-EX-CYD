export default {
  props: {
    active: Boolean
  },
  data() {
    return {
      icons: [],
      dragOver: false,
    }
  },
  computed: {
    grouped() {
      const map = new Map();
      const order = [];
      for (const icon of this.icons) {
        const name = this.iconName(icon.file);
        const offMatch = name.match(/^(.+)-off$/i);
        const onMatch  = name.match(/^(.+)-on$/i);
        const base = offMatch?.[1] ?? onMatch?.[1] ?? name;
        const slot = offMatch ? 'off' : onMatch ? 'on' : 'solo';
        if (!map.has(base)) { map.set(base, {}); order.push(base); }
        map.get(base)[slot] = icon;
      }
      return order.map(base => ({ base, ...map.get(base) }));
    }
  },
  watch: {
    active(value) {
      if (value && !this.icons.length) {
        this.load();
      }
    }
  },
  mounted() {
    this.load();
  },
  methods: {
    async load() {
      const response = await fetch('/icons');
      const list = await response.json();
      this.icons = list.map(file => ({ file }));
    },
    async uploadFiles(files) {
      for (const file of files) {
        const dest = '/icons/' + file.name;
        const raw = await file.arrayBuffer();
        const view = new DataView(raw);
        const w = view.getUint32(18, true);
        const h = Math.abs(view.getInt32(22, true));
        const bpp = view.getUint16(28, true);
        if (w > 30 || h > 30 || (bpp !== 24 && bpp !== 32)) continue;

        const processed = await this.processIcon(raw);
        const response = await fetch(dest, {
          method: 'PUT',
          headers: { 'Content-Type': 'application/octet-stream' },
          body: new Blob([processed]),
        });
        if (response.ok) {
          this.icons.push({ file: dest });
        }
      }
    },

    async processIcon(arrayBuffer) {
      // Decode BMP via canvas
      const blob = new Blob([arrayBuffer], { type: 'image/bmp' });
      const url = URL.createObjectURL(blob);
      const imgEl = await new Promise((resolve, reject) => {
        const i = new Image();
        i.onload = () => resolve(i);
        i.onerror = reject;
        i.src = url;
      });
      URL.revokeObjectURL(url);

      const canvas = document.createElement('canvas');
      canvas.width = imgEl.width;
      canvas.height = imgEl.height;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(imgEl, 0, 0);
      const imageData = ctx.getImageData(0, 0, imgEl.width, imgEl.height);

      // Convert to greyscale and invert so icon is white-on-black
      const d = imageData.data;
      for (let i = 0; i < d.length; i += 4) {
        const grey = Math.round(0.299 * d[i] + 0.587 * d[i + 1] + 0.114 * d[i + 2]);
        d[i] = d[i + 1] = d[i + 2] = 255 - grey;
      }

      return this.encodeBMP(imageData, imgEl.width, imgEl.height);
    },

    encodeBMP(imageData, w, h) {
      // 32-bit BGRA — shape pixels opaque white, background transparent
      const rowSize = w * 4; // naturally 4-byte aligned
      const pixelDataSize = rowSize * h;
      const buf = new ArrayBuffer(54 + pixelDataSize);
      const view = new DataView(buf);
      const src = imageData.data;

      // File header
      view.setUint16(0, 0x4D42, true);
      view.setUint32(2, 54 + pixelDataSize, true);
      view.setUint32(6, 0, true);
      view.setUint32(10, 54, true);

      // DIB header
      view.setUint32(14, 40, true);
      view.setInt32(18, w, true);
      view.setInt32(22, h, true);   // positive = bottom-up
      view.setUint16(26, 1, true);
      view.setUint16(28, 32, true); // 32 bpp
      view.setUint32(30, 0, true);
      view.setUint32(34, pixelDataSize, true);
      view.setInt32(38, 2835, true);
      view.setInt32(42, 2835, true);
      view.setUint32(46, 0, true);
      view.setUint32(50, 0, true);

      // Pixel data — bottom-up, BGRA: white shape + alpha=luminance
      for (let y = 0; y < h; y++) {
        const dstRow = h - 1 - y;
        for (let x = 0; x < w; x++) {
          const si = (y * w + x) * 4;
          const lum = src[si]; // greyscale — R=G=B after processIcon
          const di = 54 + dstRow * rowSize + x * 4;
          view.setUint8(di,     255); // B = white
          view.setUint8(di + 1, 255); // G = white
          view.setUint8(di + 2, 255); // R = white
          view.setUint8(di + 3, lum); // A = luminance (255=shape, 0=bg)
        }
      }

      return buf;
    },
    upload({ target }) {
      this.uploadFiles(target.files);
      target.value = '';
    },
    onDrop(e) {
      this.dragOver = false;
      const files = e.dataTransfer.files;
      if (files.length) this.uploadFiles(files);
    },
    async del(icon) {
      if (confirm('Are you sure you want to delete this icon?')) {
        const response = await fetch(icon.file, { method: 'DELETE' });
        if (response.ok) {
          this.icons = this.icons.filter(({ file }) => file !== icon.file);
        }
      }
    },
    iconName(file) {
      return file.match(/([^\/]*)\.bmp$/i)?.[1] ?? file;
    },
    canDelete(icon) {
      return icon && !icon.file.startsWith('/$/');
    }
  },
  template: `
  <div>
    <!-- Drop zone / upload area -->
    <label
      class="icon-drop-zone mb-4"
      :class="{ 'drag-active': dragOver }"
      @dragover.prevent="dragOver = true"
      @dragleave="dragOver = false"
      @drop.prevent="onDrop"
    >
      <svg width="28" height="28" fill="currentColor" class="mb-2 text-muted"><use xlink:href="bs.icons.svg#cloud-upload"/></svg>
      <span class="text-muted small">Drop BMP files here or <span class="text-accent">click to upload</span></span>
      <span class="text-muted" style="font-size: 11px;">24-bit BMP · max 30 × 30 px</span>
      <input @change="upload" type="file" accept="image/bmp" multiple class="d-none" />
    </label>

    <!-- Paired grid -->
    <div v-if="grouped.length" class="icon-pair-grid">
      <div v-for="group of grouped" :key="group.base" class="icon-pair-card">
        <span class="icon-pair-label">{{ group.base }}</span>
        <div class="icon-pair-cells">

          <!-- Off state (or solo) -->
          <div v-if="group.off || group.solo" class="icon-cell">
            <img :src="(group.off ?? group.solo).file" />
            <span class="icon-state-badge">{{ group.solo ? '' : 'Off' }}</span>
            <button v-if="canDelete(group.off ?? group.solo)" @click="del(group.off ?? group.solo)" class="icon-delete" title="Delete">
              <svg width="12" height="12" fill="currentColor"><use xlink:href="bs.icons.svg#x-lg"/></svg>
            </button>
          </div>

          <!-- On state -->
          <div v-if="group.on" class="icon-cell">
            <img :src="group.on.file" />
            <span class="icon-state-badge">On</span>
            <button v-if="canDelete(group.on)" @click="del(group.on)" class="icon-delete" title="Delete">
              <svg width="12" height="12" fill="currentColor"><use xlink:href="bs.icons.svg#x-lg"/></svg>
            </button>
          </div>

        </div>
      </div>
    </div>

    <div v-else class="text-center text-muted py-5 small">
      No icons uploaded yet — drop a BMP file above to get started.
    </div>
  </div>
  `
}
