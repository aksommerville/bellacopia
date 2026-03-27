/* EditSaveModal.js
 * For adulterating or generating saved games.
 */
 
import { Dom } from "../js/Dom.js";
import { Data } from "../js/Data.js";
import { Actions } from "../js/Actions.js";
import { SharedSymbols } from "../js/SharedSymbols.js";
import { MapService } from "../js/map/MapService.js";
import { EditSaveInventoryModal } from "./EditSaveInventoryModal.js";
import { EditSaveJigsawModal } from "./EditSaveJigsawModal.js";

export class EditSaveModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data, Window, MapService, SharedSymbols, Actions, "nonce"];
  }
  constructor(element, dom, data, window, mapService, sharedSymbols, actions, nonce) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.mapService = mapService;
    this.sharedSymbols = sharedSymbols;
    this.actions = actions;
    this.nonce = nonce;
    
    this.fileName = "bellacopia.save";
    this.rawTextDirty = false;
    this.contentDirty = false;
    
    this.sharedSymbols.whenLoaded().then(() => {
      this.buildUi();
    });
  }
  
  /* UI.
   ************************************************************************/
   
  buildUi() {
    this.element.innerHTML = "";
    
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    this.dom.spawn(topRow, "INPUT", { type: "file", "on-change": e => this.onFile(e) });
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "Save...", "on-click": () => this.onSave() });
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "Clear", "on-click": () => this.onClear() });
    this.dom.spawn(topRow, "INPUT", { type: "button", value: "Full", "on-click": () => this.onFull() });
    
    /* rawText and content dirty each other, not themselves.
     * "dirty" means that section is out of date and needs to be regenerated from the other section.
     * Since those conversions are expensive, we defer until forced.
     */
    this.dom.spawn(this.element, "TEXTAREA", { name: "rawText", "on-input": e => this.onContentDirty(e) });
    const tableScroller = this.dom.spawn(this.element, "DIV", ["tableScroller"]);
    const table = this.dom.spawn(tableScroller, "TABLE", ["content"], { "on-input": () => this.onRawTextDirty() });
    this.buildTable(table);
  }
  
  createRawTextDirtyUi() {
    this.element.querySelector(".rawTextDirty")?.remove();
    const element = this.dom.spawn(this.element, "DIV", ["rawTextDirty"]);
    const rawText = this.element.querySelector("textarea[name='rawText']");
    const bounds = rawText.getBoundingClientRect();
    const pbounds = this.element.getBoundingClientRect();
    element.style.left = (bounds.left - pbounds.left) + "px";
    element.style.top = (bounds.top - pbounds.top) + "px";
    element.style.width = bounds.width + "px";
    element.style.height = bounds.height + "px";
    this.dom.spawn(element, "INPUT", { type: "button", value: "Encode", "on-click": () => this.onRemakeRawText() });
  }
  
  createContentDirtyUi() {
    this.element.querySelector(".contentDirty")?.remove();
    const element = this.dom.spawn(this.element, "DIV", ["contentDirty"]);
    const table = this.element.querySelector(".tableScroller");
    const bounds = table.getBoundingClientRect();
    const pbounds = this.element.getBoundingClientRect();
    element.style.left = (bounds.left - pbounds.left) + "px";
    element.style.top = (bounds.top - pbounds.top) + "px";
    element.style.width = bounds.width + "px";
    element.style.height = bounds.height + "px";
    this.dom.spawn(element, "INPUT", { type: "button", value: "Decode", "on-click": () => this.onRemakeContent() });
  }
  
  dropDirty() {
    this.rawTextDirty = false;
    this.contentDirty = false;
    this.element.querySelector(".rawTextDirty")?.remove();
    this.element.querySelector(".contentDirty")?.remove();
  }
  
  buildTable(table) {
    table.innerHTML = "";
    
    /* All these lists are {k,v,comment?}, straight off SharedSymbols.
     * They should be segregated to begin with, but we're not counting on it.
     */
    const fldv = [];
    const fld16v = [];
    const clockv = [];
    for (const sym of this.sharedSymbols.symv) {
      if (sym.nstype !== "NS") continue;
      let dst = null;
      switch (sym.ns) {
        case "fld": dst = fldv; break;
        case "fld16": dst = fld16v; break;
        case "clock": dst = clockv; break;
      }
      if (dst) {
        const record = { k: sym.k, v: sym.v };
        if (sym.comment) record.comment = sym.comment;
        dst.push(record);
      }
    }
    fldv.sort((a, b) => a.v - b.v);
    fld16v.sort((a, b) => a.v - b.v);
    clockv.sort((a, b) => a.v - b.v);
    
    /* Build up an array of orphan input elements.
     */
    let idNext = 1;
    const nextId = () => `ESM-${this.nonce}-input-${idNext++}`;
    const inputs = [];
    for (const { k, v, comment } of clockv) {
      const input = this.dom.spawn(null, "INPUT", { type: "text", name: k, "data-store": "clock", "data-index": v, id: nextId() });
      if (comment) input.setAttribute("data-comment", comment);
      inputs.push(input);
    }
    // Buttons to open a sub-modal for inventory and jigsaw.
    // Putting these between clocks and fld16s just to create some separation between those.
    inputs.push(this.dom.spawn(null, "INPUT", { type: "button", name: "Inventory", value: "Edit...", "data-store": "invstore", "on-click": () => this.onEditInventory(), id: nextId() }));
    inputs.push(this.dom.spawn(null, "INPUT", { type: "button", name: "Jigsaw", value: "Edit...", "data-store": "jigstore", "on-click": () => this.onEditJigsaw(), id: nextId() }));
    // And onward with the generics.
    for (const { k, v, comment } of fld16v) {
      const input = this.dom.spawn(null, "INPUT", { type: "number", name: k, min: 0, max: 65535, "data-store": "fld16", "data-index": v, id: nextId() });
      if (comment) input.setAttribute("data-comment", comment);
      inputs.push(input);
    }
    for (const { k, v, comment } of fldv) {
      const input = this.dom.spawn(null, "INPUT", { type: "checkbox", name: k, "data-store": "fld", "data-index": v, id: nextId() });
      if (comment) input.setAttribute("data-comment", comment);
      inputs.push(input);
    }
    
    /* Split into columns of equal length. Last column is short.
     */
    const colc = 4;
    const rowc = Math.ceil(inputs.length / colc);
    const pv = [];
    for (let i=0; i<colc; i++) pv.push(i * rowc);
    
    /* Add to table rowwise.
     */
    for (let row=0; row<rowc; row++) {
      const tr = this.dom.spawn(table, "TR");
      for (let col=0; col<colc; col++) {
        const input = inputs[pv[col]++];
        if (input) {
          const tdk = this.dom.spawn(tr, "TD", ["key"],
            this.dom.spawn(null, "LABEL", { for: input.id }, input.name)
          );
          const tdv = this.dom.spawn(tr, "TD", ["value"], input);
          const comment = input.getAttribute("data-comment");
          if (comment) {
            tdk.setAttribute("title", comment);
            tdv.setAttribute("title", comment);
          }
        } else {
          this.dom.spawn(tr, "TD");
          this.dom.spawn(tr, "TD");
        }
      }
    }
  }
  
  /* Model.
   **************************************************************************/
  
  // Resolves with the base64-encoded saved game, for an Egg store file.
  decodeBinary(src) {
    return new Promise((resolve, reject) => {
      try {
        const store = this.decodeEggStoreFile(src);
        const keys = Object.keys(store);
        if (!keys.length) {
          resolve("");
        } else if (keys.length > 1) {
          this.dom.modalPickOne("Select saved game:", keys).then(rsp => {
            if (!rsp) resolve("");
            else resolve(keys[rsp]);
          }).catch(e => reject(e));
        } else {
          resolve(store[keys[0]]);
        }
      } catch (e) {
        reject(e);
      }
    });
  }
  
  // Returns an object where each member is a valid-looking saved game.
  // At this writing, there will only be one, called "save", but we're leaving the door open to multi-game save files.
  decodeEggStoreFile(src) {
    if (!src) return {};
    if (src instanceof ArrayBuffer) src = new Uint8Array(src);
    if (!(src instanceof Uint8Array)) throw new Error(`Expected Uint8Array`);
    const store = {};
    const textDecoder = new this.window.TextDecoder("utf8");
    for (let srcp=0; srcp<src.length; ) {
      const kc = src[srcp++] || 0;
      const vc = ((src[srcp] << 8) | src[srcp+1]) || 0; srcp += 2;
      if (srcp > src.length - vc - kc) throw new Error(`Malformed save file.`);
      const k = textDecoder.decode(src.slice(srcp, srcp + kc)); srcp += kc;
      const v = textDecoder.decode(src.slice(srcp, srcp + vc)); srcp += vc;
      if (!v.match(/^[0-9a-zA-Z+/]+$/)) {
        console.warn(`Ignoring invalid field ${JSON.stringify(k)} in save file: ${JSON.stringify(v)}`);
        continue;
      }
      if (store.hasOwnProperty(k)) throw new Error(`Duplicate key ${JSON.stringify(k)} in save file.`);
      store[k] = v;
    }
    return store;
  }
  
  // Returns Uint8Array of a valid Egg store file, with a single member.
  encodeEggStoreFile(k, v) {
    k = this.sanitizeKeyForEggStore(k);
    v = this.sanitizeValueForEggStore(v);
    const len = 3 + k.length + v.length;
    const dst = new Uint8Array(len);
    dst[0] = k.length;
    dst[1] = v.length >> 8;
    dst[2] = v.length;
    const encoder = new this.window.TextEncoder("utf8");
    new Uint8Array(dst.buffer, 3, k.length).set(encoder.encode(k));
    new Uint8Array(dst.buffer, 3 + k.length, v.length).set(encoder.encode(v));
    return dst;
  }
  
  sanitizeKeyForEggStore(src) {
    src = src.trim();
    if (src.length > 0xff) return src.substring(0, 0xff);
    return src;
  }
  
  sanitizeValueForEggStore(src) {
    src = src.replace(/[^0-9a-zA-Z+/]/g, "");
    if (src.length > 0xffff) return src.substring(0, 0xffff);
    return src;
  }
  
  /* Produce a live model, which we only use transiently, when populating the loose fields from the raw text.
   * Format is defined in src/game/store.h, and I'll summarize here.
   * The whole thing is Base64, but you have to decode it in little bits.
   * Starts with 10 bytes of TOC. Each entry is 2 digits, ie 0..0xfff, and the meaning of each count is different per store.
   *  - fldv: len=bytes decoded (multiple of 3). Straight Base64, and length must align to a block.
   *  - fld16v: len=fields. Three encoded bytes each, big-endian, the 2 high bits of each must be zero.
   *  - clockv: len=fields. Five encoded bytes each, big-endian, ms. Holds about 298 hours each.
   *  - jigstorev: len=records. Five encoded bytes each, split big-endianly: 11 mapid, 8 x, 8 y, 3 xform.
   *  - invstorev: len=records. Four encoded bytes each: itemid,limit,quantity. ie straight base64 of the whole (invstorev).
   */
  modelFromText(src) {
    src = src.trim();
    const model = {
      fld: [], // Zero or one, index is id.
      fld16: [], // 0..0xffff, index is id.
      clock: [], // Integer ms, index is id.
      jigstore: [], // {mapid,x,y,xform}. Indexed preserved but not really meaningful. (it's the display order at runtime)
      invstore: [], // {itemid,limit,quantity}. Indexed by runtime invstore position 0..25. [0] is the equipped item.
    };
    if (src?.length >= 10) {
      const b64 = (p, c) => {
        let v = 0;
        for (; c-->0; p++) {
          let digit = src.charCodeAt(p);
               if ((digit >= 0x41) && (digit <= 0x5a)) digit = digit - 0x41;
          else if ((digit >= 0x61) && (digit <= 0x7a)) digit = digit - 0x61 + 26;
          else if ((digit >= 0x30) && (digit <= 0x39)) digit = digit - 0x30 + 52;
          else if (digit === 0x2b) digit = 62;
          else if (digit === 0x2f) digit = 63;
          else throw new Error(`Illegal byte ${digit} in Base64-encoded saved game.`);
          v <<= 6;
          v |= digit;
        }
        return v;
      };
      
      // TOC
      let fldc = b64(0, 2);
      const fld16c = b64(2, 2);
      const clockc = b64(4, 2);
      const jigstorec = b64(6, 2);
      const invstorec = b64(8, 2);
      let srcp = 10;
      
      // fld
      if (srcp > src.length - fldc) throw new Error(`fld overrun`);
      for (let i=fldc; i-->0; ) {
        const v = b64(srcp, 1);
        srcp += 1;
        for (let mask=1; mask<0x40; mask<<=1) {
          model.fld.push((v & mask) ? 1 : 0);
        }
      }
      
      // fld16
      if (srcp > src.length - fld16c * 3) throw new Error(`fld16 overrun`);
      for (let i=fld16c; i-->0; ) {
        const v = b64(srcp, 3);
        srcp += 3;
        if (v & ~0xffff) throw new Error(`invalid fld16: ${v}`);
        model.fld16.push(v);
      }
      
      // clock
      if (srcp > src.length - clockc * 5) throw new Error(`clock overrun`);
      for (let i=clockc; i-->0; ) {
        const v = b64(srcp, 5);
        srcp += 5;
        model.clock.push(v);
      }
      
      // jigstore
      if (srcp > src.length - jigstorec * 5) throw new Error(`jigstore overrun`);
      for (let i=jigstorec; i-->0; ) {
        const v = b64(srcp, 5);
        srcp += 5;
        const mapid = v >> 19;
        const x = (v >> 11) & 0xff;
        const y = (v >> 3) & 0xff;
        const xform = v & 0x7;
        model.jigstore.push({ mapid, x, y, xform });
      }
      
      // invstore
      if (srcp > src.length - invstorec * 4) throw new Error(`invstore overrun`);
      for (let i=invstorec; i-->0; ) {
        const v = b64(srcp, 4);
        srcp += 4;
        const itemid = v >> 16;
        const limit = (v >> 8) & 0xff;
        const quantity = v & 0xff;
        model.invstore.push({ itemid, limit, quantity });
      }
      
      // checksum and length
      if (srcp !== src.length - 5) {
        console.warn(`Unexpected final position ${srcp} of ${src.length}. Skipping checksum.`);
      } else {
        const declared = b64(srcp, 5);
        let actual = 0;
        for (let i=0; i<srcp; i++) {
          actual = (actual >>> 31) | (actual << 1);
          actual ^= src.charCodeAt(i);
        }
        actual &= 0x3fffffff;
        if (actual !== declared) {
          console.warn(`Checksum mismatch. Actual = ${actual}, declared = ${declared}. Proceeding anyway.`);
        }
        srcp += 5;
      }
    }
    return model;
  }
  
  /* Produce text from a model pulled off the UI.
   */
  textFromModel(model) {
  
    /* Determine minimal TOC.
     */
    let fldc = model.fld.length;
    while (fldc && !model.fld[fldc-1]) fldc--;
    const fldc_encoded = Math.ceil(fldc / 6);
    let fld16c = model.fld16.length;
    while (fld16c && !model.fld16[fld16c-1]) fld16c--;
    let clockc = model.clock.length;
    while (clockc && !model.clock[clockc-1]) clockc--;
    let jigc = model.jigstore.length;
    while (jigc && !model.jigstore[jigc-1].mapid) jigc--;
    let invc = model.invstore.length;
    while (invc && !model.invstore[invc-1].itemid) invc--;
    
    /* Allocate output buffer and emit TOC.
     */
    const total = 10 + fldc_encoded + fld16c * 3 + clockc * 5 + jigc * 5 + invc * 4 + 5;
    const dst = new Uint8Array(total);
    let dstc = 0;
    const alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/".split("").map(v => v.charCodeAt(0));
    const b64 = (c, v) => { // c in 1..5
      for (let i=c; i-->0; v>>=6) {
        dst[dstc + i] = alphabet[v & 0x3f];
      }
      dstc += c;
    };
    b64(2, fldc_encoded);
    b64(2, fld16c);
    b64(2, clockc);
    b64(2, jigc);
    b64(2, invc);
    
    /* Encode the payloads.
     */
    for (let i=fldc_encoded, fldp=0; i-->0; ) {
      let v = 0;
      for (let shift=0; shift<6; shift++) {
        v |= model.fld[fldp++] << shift;
      }
      dst[dstc++] = alphabet[v];
    }
    for (let i=0; i<fld16c; i++) {
      b64(3, model.fld16[i]);
    }
    for (let i=0; i<clockc; i++) {
      b64(5, model.clock[i]);
    }
    for (let i=0; i<jigc; i++) {
      const j = model.jigstore[i];
      const v = (j.mapid << 19) | (j.x << 11) | (j.y << 3) | j.xform;
      b64(5, v);
    }
    for (let i=0; i<invc; i++) {
      const inv = model.invstore[i];
      const v = (inv.itemid << 16) | (inv.limit << 8) | inv.quantity;
      b64(4, v);
    }
    
    /* Checksum across the encoded content produced so far.
     */
    let sum = 0;
    for (let i=0; i<dstc; i++) {
      sum = (sum >>> 31) | (sum << 1);
      sum ^= dst[i];
    }
    sum &= 0x3fffffff;
    b64(5, sum);
    
    /* Assert length, then encode to a string.
     */
    if (dstc !== total) {
      throw new Error(`Encoding failed. Expected ${total} bytes but produced ${dstc}`);
    }
    const decoder = new this.window.TextDecoder("utf8");
    return decoder.decode(dst);
  }
  
  textFromContentUi() {
    const model = {
      fld: [], // Zero or one, index is id.
      fld16: [], // 0..0xffff, index is id.
      clock: [], // Integer ms, index is id.
      jigstore: [], // {mapid,x,y,xform}. Indexed preserved but not really meaningful. (it's the display order at runtime)
      invstore: [], // {itemid,limit,quantity}. Indexed by runtime invstore position 0..25. [0] is the equipped item.
    };
    for (const input of this.element.querySelectorAll(`.content input`)) {
      switch (input.getAttribute("data-store")) {
        case "fld": if (input.checked) {
            const p = +input.getAttribute("data-index") || 0;
            while (model.fld.length <= p) model.fld.push(0);
            model.fld[p] = 1;
          } break;
        case "fld16": if (+input.value) {
            const p = +input.getAttribute("data-index") || 0;
            while (model.fld16.length <= p) model.fld16.push(0);
            model.fld16[p] = +input.value;
          } break;
        case "clock": if (+input.value) {
            const p = +input.getAttribute("data-index") || 0;
            while (model.clock.length <= p) model.clock.push(0);
            model.clock[p] = +input.value; //TODO eval?
          } break;
        case "jigstore": {
            model.jigstore = JSON.parse(input.getAttribute("data-json"));
          } break;
        case "invstore": {
            model.invstore = JSON.parse(input.getAttribute("data-json"));
          } break;
        default: console.log(`unknown store for input`, input);
      }
    }
    return this.textFromModel(model);
  }
  
  populateContentFromText() {
    const text = this.element.querySelector("textarea[name='rawText']").value;
    const model = this.modelFromText(text);
    for (const input of this.element.querySelectorAll(`.content input`)) {
      switch (input.getAttribute("data-store")) {
        case "fld": {
            if (model.fld[+input.getAttribute("data-index")]) {
              input.checked = true;
            } else {
              input.checked = false;
            }
          } break;
        case "fld16": {
            input.value = model.fld16[+input.getAttribute("data-index")] || "";
          } break;
        case "clock": {
            input.value = model.clock[+input.getAttribute("data-index")] || "";//TODO repr?
          } break;
        // We probably shouldn't have bothered decoding jigstore and invstore. They're going to be managed by other widgets.
        case "jigstore": {
            input.setAttribute("data-json", JSON.stringify(model.jigstore));
          } break;
        case "invstore": {
            input.setAttribute("data-json", JSON.stringify(model.invstore));
          } break;
        default: console.log(`unknown store for input`, input);
      }
    }
  }
  
  /* Events.
   *************************************************************************/
   
  onFile(event) {
    if (event?.target?.files?.length !== 1) return;
    const file = event.target.files[0];
    file.stream().getReader().read()
      .then(content => this.decodeBinary(content.value))
      .then(text => {
        this.dropDirty();
        const rawText = this.element.querySelector("textarea[name='rawText']");
        rawText.value = text;
        this.onContentDirty({ target: rawText });
        this.forceClean();
        this.fileName = file.name;
      }).catch(e => this.dom.modalError(e));
  }
  
  onClear() {
    const rawText = this.element.querySelector("textarea[name='rawText']");
    if (!rawText) return;
    rawText.value = "";
    this.onContentDirty({ target: rawText });
    this.forceClean();
  }
  
  onFull() {
    console.log(`EditSaveModal.onFull`);//TODO pretty subjective. Get the rest working first. Actually... What do we need this for?
  }
  
  onContentDirty() {
    if (this.rawTextDirty) {
      console.warn(`rawTextDirty and contentDirty are both trying to be true.`);
      return;
    }
    if (this.contentDirty) return;
    this.contentDirty = true;
    this.createContentDirtyUi();
  }
  
  onRawTextDirty(event) {
    if (this.contentDirty) {
      console.warn(`rawTextDirty and contentDirty are both trying to be true.`);
      return;
    }
    if (this.rawTextDirty) return;
    this.rawTextDirty = true;
    this.createRawTextDirtyUi();
  }
  
  onRemakeRawText() {
    const text = this.textFromContentUi();
    this.element.querySelector("textarea[name='rawText']").value = text;
    this.dropDirty();
  }
  
  onRemakeContent() {
    const text = this.element.querySelector("textarea[name='rawText']").value;
    this.populateContentFromText(text);
    this.dropDirty();
  }
  
  forceClean() {
    if (this.rawTextDirty) this.onRemakeRawText();
    if (this.contentDirty) this.onRemakeContent();
    this.dropDirty();
  }
  
  onSave() {
    this.forceClean();
    const text = this.element.querySelector("textarea[name='rawText']")?.value || "";
    const bin = this.encodeEggStoreFile("save", text);
    const blob = new Blob([bin], { type: "application/octet-stream" });
    const url = this.window.URL.createObjectURL(blob);
    // This is pretty dumb. We have to simulate a click of an <a> element to control the proposed file name. Blob and URL can't do that on their own.
    const a = this.dom.document.createElement("A");
    a.href = url;
    a.download = this.fileName;
    a.click();
    // Chrome Linux, this does appear to be safe. If you find otherwise, remove this line and let it leak.
    this.window.URL.revokeObjectURL(url);
  }
  
  onEditInventory() {
    const json = this.element.querySelector("input[name='Inventory']")?.getAttribute("data-json") || "";
    const modal = this.dom.spawnModal(EditSaveInventoryModal);
    modal.setup(json);
    modal.result.then(rsp => {
      if (typeof(rsp) !== "string") return;
      this.element.querySelector("input[name='Inventory']")?.setAttribute("data-json", rsp);
      this.onRawTextDirty();
    }).catch(e => this.dom.modalError(e));
  }
  
  onEditJigsaw() {
    const json = this.element.querySelector("input[name='Jigsaw']")?.getAttribute("data-json") || "";
    const modal = this.dom.spawnModal(EditSaveJigsawModal);
    modal.setup(json);
    modal.result.then(rsp => {
      if (typeof(rsp) !== "string") return;
      this.element.querySelector("input[name='Jigsaw']")?.setAttribute("data-json", rsp);
      this.onRawTextDirty();
    }).catch(e => this.dom.modalError(e));
  }
}
