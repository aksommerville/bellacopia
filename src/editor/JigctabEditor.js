/* JigctabEditor.js
 * Alternate editor for tilesheet resources, that gives you neat RGB editing of the jigsaw color table.
 */
 
import { Dom } from "./js/Dom.js";
import { Data } from "./js/Data.js";
import { Rgb332Modal } from "./Rgb332Modal.js";

export class JigctabEditor {
  static getDependencies() {
    return [HTMLElement, Dom, Data, Window];
  }
  constructor(element, dom, data, window) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    
    this.res = null;
    this.image = null;
    this.ctab = []; // "#RRGGBB", ready for canvas to use.
    this.renderTimeout = null;
    this.selp = -1;
  }
  
  static checkResource(res) {
    if (res.type === "tilesheet") return 1;
    return 0;
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
  }
  
  setup(res) {
    this.res = res;
    try {
      this.ctab = this.decode(this.res.serial);
    } catch (e) {
      console.error(e);
      this.ctab = [];
    }
    this.data.getImageAsync(res.rid).then(image => {
      this.image = image;
      this.buildUi();
    }).catch(e => this.dom.modalError(e));
  }
  
  buildUi() {
    this.element.innerHTML = "";
    const canvas = this.dom.spawn(this.element, "CANVAS", ["visual"], {
      width: this.image.naturalWidth,
      height: this.image.naturalHeight,
      "on-click": e => this.onClick(e),
    });
    this.renderSoon();
  }
  
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector(".visual");
    const ctx = canvas.getContext("2d");
    ctx.drawImage(this.image, 0, 0, canvas.width, canvas.height, 0, 0, canvas.width, canvas.height);
    for (let dsty=8, yi=16, ctabp=0; yi-->0; dsty+=16) {
      for (let dstx=8, xi=16; xi-->0; dstx+=16, ctabp++) {
        ctx.fillStyle = this.ctab[ctabp] || "#000";
        ctx.fillRect(dstx, dsty, 8, 8);
        if (ctabp === this.selp) {
          ctx.fillStyle = "#ff08";
          ctx.fillRect(dstx-8, dsty-8, 16, 16);
        }
      }
    }
  }
  
  onClick(event) {
    const canvas = event.target;
    const bounds = canvas.getBoundingClientRect();
    let x = event.clientX - bounds.x;
    let y = event.clientY - bounds.y;
    let w;
    // We're using `object-fit:contain`, which is going to make this awkward. Luckily, we know the content is exactly square.
    if (bounds.width > bounds.height) {
      w = bounds.height;
      x -= (bounds.width - w) * 0.5;
    } else {
      w = bounds.width;
      y -= (bounds.height - w) * 0.5;
    }
    const col = Math.floor((x * 16) / w);
    const row = Math.floor((y * 16) / w);
    if ((col < 0) || (row < 0) || (col > 15) || (row > 15)) return;
    if (event.ctrlKey) {
      this.selp = (row << 4) | col;
      this.renderSoon();
    } else if (event.shiftKey) {
      if (this.selp >= 0) {
        this.ctab[(row << 4) | col] = this.ctab[this.selp];
        this.renderSoon();
        this.data.dirty(this.res.path, () => this.encode(this.ctab));
      }
    } else {
      this.editCell((row << 4) | col);
    }
  }
  
  editCell(p) {
    const modal = this.dom.spawnModal(Rgb332Modal);
    modal.setup(this.ctab[p]);
    modal.result.then(rsp => {
      if (!rsp) return;
      let r = rsp & 0xe0; r |= r >> 3; r |= r >> 6;
      let g = rsp & 0x1c; g |= g << 3; g |= g >> 6;
      let b = rsp & 0x03; b |= b << 2; b |= b << 4;
      const css = "#" + r.toString(16).padStart(2, '0') + g.toString(16).padStart(2, '0') + b.toString(16).padStart(2, '0');
      this.ctab[p] = css;
      this.renderSoon();
      this.data.dirty(this.res.path, () => this.encode(this.ctab));
    }).catch(e => this.dom.modalError(e));
  }
  
  encode(ctab) {
    let dst = "";
    const src = new TextDecoder("utf8").decode(this.res.serial);
    let skip = false, emitted = false;
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) {
        dst += line + "\n";
        skip = false;
        continue;
      }
      if (line === "jigctab") {
        skip = true;
        dst += "jigctab\n";
        dst += this.encodeTable(ctab);
        dst += "\n";
        emitted = true;
      } else if (skip) {
      } else {
        dst += line + "\n";
      }
    }
    if (!emitted) {
      dst += "jigctab\n";
      dst += this.encodeTable(ctab);
    }
    return new TextEncoder("utf8").encode(dst);
  }
  
  encodeTable(src) {
    let dst = "";
    for (let row=0, srcp=0; row<16; row++) {
      for (let col=0; col<16; col++, srcp++) {
        const css = src[srcp] || ""
        const r = parseInt(css.substring(1, 3), 16) || 0;
        const g = parseInt(css.substring(3, 5), 16) || 0;
        const b = parseInt(css.substring(5, 7), 16) || 0;
        const rgb332 = (r & 0xe0) | ((g >> 3) & 0x1c) | (b >> 6);
        dst += "0123456789abcdef"[rgb332 >> 4];
        dst += "0123456789abcdef"[rgb332 & 15];
      }
      dst += "\n";
    }
    return dst;
  }
  
  decode(src) {
    src = new TextDecoder("utf8").decode(src);
    let row = -1;
    const dst = [];
    for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
      let nlp = src.indexOf("\n", srcp);
      if (nlp < 0) nlp = src.length;
      const line = src.substring(srcp, nlp).trim();
      srcp = nlp + 1;
      if (!line || line.startsWith("#")) continue;
      if (line === "jigctab") {
        row = 0;
      } else if (row >= 0) {
        for (let linep=0; linep<32; linep+=2) {
          const rgb332 = parseInt(line.substring(linep, linep+2), 16);
          let r = rgb332 & 0xe0; r |= r >> 3; r |= r >> 6;
          let g = rgb332 & 0x1c; g |= g << 3; g |= g >> 6;
          let b = rgb332 & 0x03; b |= b << 2; b |= b << 4;
          dst.push("#" + r.toString(16).padStart(2, '0') + g.toString(16).padStart(2, '0') + b.toString(16).padStart(2, '0'));
        }
        if (++row >= 16) return dst;
      }
    }
    return dst;
  }
}
