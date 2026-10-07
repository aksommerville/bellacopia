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
import { SavedGame } from "./SavedGame.js";

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
    this.savedGame = null;
    
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
  
  textFromContentUi() {
    this.mapService.requireLayout();
    const model = new SavedGame(null);
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
            model.clock[p] = +input.value;
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
    return model.sortJigstore().packJigstore(this.mapService).encode();
  }
  
  populateContentFromText() {
    this.mapService.requireLayout();
    const text = this.element.querySelector("textarea[name='rawText']").value;
    const model = new SavedGame(text).unpackJigstore(this.mapService);
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
            input.value = model.clock[+input.getAttribute("data-index")] || "";
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
