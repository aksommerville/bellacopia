/* EditSaveJigsawModal.js
 * Shows all the jigsawed planes and lets you toggle each map.
 * This only controls whether present or absent. We're not exposing control over position or transform.
 * If you toggle a map off and on again, its position and transform will be randomized.
 */
 
import { Dom } from "../js/Dom.js";
import { SharedSymbols } from "../js/SharedSymbols.js";
import { MapService } from "../js/map/MapService.js";
import { Data } from "../js/Data.js";
import { Tilesheet } from "../js/std/Tilesheet.js";

export class EditSaveJigsawModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SharedSymbols, Window, MapService, Data];
  }
  constructor(element, dom, sharedSymbols, window, mapService, data) {
    this.element = element;
    this.dom = dom;
    this.sharedSymbols = sharedSymbols;
    this.window = window;
    this.mapService = mapService;
    this.data = data;
    
    this.mapw = this.sharedSymbols.getValue("NS", "sys", "mapw") || 20;
    this.maph = this.sharedSymbols.getValue("NS", "sys", "maph") || 12;
    
    this.ctabv = []; // Indexed by tilesheet id (aka image id); each member is 256 CSS strings.
    this.acquireColorTables();
    
    this.layers = []; // MapService.layer, plus (name), (comment), (img).
    this.acquireMaps();
    this.renderTimeout = null;
    
    // Resolve with null if cancelled or unchanged. Otherwise with the new JSON text.
    this.model = []; // {mapid,x,y,xform}
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
      this.renderTimeout = null;
    }
    this.resolve(null);
  }
  
  setup(src) {
    this.model = JSON.parse(src || "[]");
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    this.dom.spawn(this.element, "DIV", ["advice"], "Only possession is editable, not position or transform.");
    this.dom.spawn(this.element, "DIV", ["row"],
      this.dom.spawn(null, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() })
    );
    
    /* Canvas for each layer.
     */
    for (const layer of this.layers) {
      if (!layer.img) continue;
      const canvas = this.dom.spawn(this.element, "CANVAS", ["jigsaw"], { "data-z": layer.z, "on-click": e => this.onClickCanvas(e) });
      canvas.width = layer.img.width;
      canvas.height = layer.img.height;
    }
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
    for (const canvas of this.element.querySelectorAll("canvas[data-z]")) {
      const z = +canvas.getAttribute("data-z");
      const layer = this.layers.find(l => l.z === z);
      if (!layer) continue;
      const ctx = canvas.getContext("2d");
      
      // Fill my canvas with the prepared puzzle image.
      ctx.drawImage(layer.img, 0, 0);
      
      // Draw some transparentish gray over the ones not possessed.
      ctx.fillStyle = "#444c";
      for (let y=0, yi=layer.h, p=0; yi-->0; y+=this.maph+1) {
        for (let x=0, xi=layer.w; xi-->0; x+=this.mapw+1, p++) {
          const res = layer.v[p];
          if (!res || !res.map) continue;
          const map = res.map;
          if (this.model.find(j => j.mapid === map.rid)) continue; // Leave colorful.
          ctx.fillRect(x, y, this.mapw, this.maph);
        }
      }
    }
  }
  
  /* Events.
   ****************************************************************************/
   
  onClickCanvas(event) {
    const canvas = event?.target;
    if (!canvas) return;
    const z = +canvas.getAttribute("data-z");
    const layer = this.layers.find(l => l.z === z);
    if (!layer) return;
    const bounds = canvas.getBoundingClientRect();
    const nx = (event.clientX - bounds.left) / bounds.width;
    const ny = (event.clientY - bounds.top) / bounds.height;
    const lng = Math.floor(nx * layer.w);
    const lat = Math.floor(ny * layer.h);
    if ((lng < 0) || (lng >= layer.w) || (lat < 0) || (lat >= layer.h)) return;
    const res = layer.v[lat * layer.w + lng];
    if (!res || !res.map) return;
    const modelp = this.model.findIndex(m => m.mapid === res.map.rid);
    if (modelp >= 0) {
      this.model.splice(modelp, 1);
    } else {
      // Only half of the possible xforms are legal: Those with an even count of bits set.
      let xform = Math.floor(Math.random() * 4);
      switch (xform) {
        case 0: xform = 0; break;
        case 1: xform = 3; break;
        case 2: xform = 5; break;
        default: xform = 6; break;
      }
      this.model.push({
        mapid: res.map.rid,
        x: Math.floor(Math.random() * 236), // JIGSAW_FLDW
        y: Math.floor(Math.random() * 153), // JIGSAW_FLDH
        xform,
      });
    }
    this.renderSoon();
  }
  
  onSubmit() {
    this.resolve(JSON.stringify(this.model));
    this.element.remove();
  }
  
  /* Maps acquisition, during construction.
   *****************************************************************************************/
   
  acquireColorTables() {
    this.ctabv = [];
    for (const res of this.data.resv) {
      if (res.type !== "tilesheet") continue;
      const tilesheet = new Tilesheet(res.serial);
      const src = tilesheet.tables.jigctab;
      if (!src) continue;
      const dst = [];
      for (let i=0; i<256; i++) {
        const rgb332 = src[i];
        let r = rgb332 & 0xe0; r |= r >> 3; r |= r >> 6;
        let g = rgb332 & 0x1c; g |= g << 3; g |= g >> 6;
        let b = rgb332 & 0x03; b |= b << 2; b |= b << 4;
        const css = "#" + r.toString(16).padStart(2, '0') + g.toString(16).padStart(2, '0') + b.toString(16).padStart(2, '0');
        dst.push(css);
      }
      this.ctabv[res.rid] = dst;
    }
  }
   
  acquireMaps() {
  
    /* Find all the layers in SharedSymbols that don't declare "NOJIGSAW".
     * Fill (this.layers) with cropped MapService layers, adding (name) and (comment) from SharedSymbols.
     */
    this.layers = [];
    for (const sym of this.sharedSymbols.symv) {
      if (sym.ns !== "plane") continue;
      if (sym.nstype !== "NS") continue;
      if ((sym.comment || "").indexOf("NOJIGSAW") >= 0) continue;
      let layer = this.mapService.layout.find(l => l.z === sym.v);
      if (layer) layer = layer.crop();
      else layer = { z: sym.v, w: 0, h: 0, v: [] };
      layer.name = sym.k;
      layer.comment = sym.comment || "";
      this.layers.push(layer);
    }
    
    /* Draw each layer, roughly the same way runtime will.
     * Don't bother making nubbins, and include a border around each map.
     * If the layer is empty, don't create an image.
     */
    for (const layer of this.layers) {
      if ((layer.w < 1) || (layer.h < 1)) continue;
      const imgw = layer.w * (this.mapw + 1);
      const imgh = layer.h * (this.maph + 1);
      layer.img = this.dom.document.createElement("CANVAS");
      layer.img.width = imgw;
      layer.img.height = imgh;
      const ctx = layer.img.getContext("2d");
      for (let yi=layer.h, dsty=0, p=0; yi-->0; dsty+=this.maph+1) {
        for (let xi=layer.w, dstx=0; xi-->0; dstx+=this.mapw+1, p++) {
          const map = layer.v[p]?.map;
          if (map) {
            this.drawMap(layer.img, ctx, dstx, dsty, map);
          }
        }
      }
      ctx.beginPath();
      for (let xi=layer.w, dstx=this.mapw+0.5; xi-->0; dstx+=this.mapw+1) {
        ctx.moveTo(dstx, 0);
        ctx.lineTo(dstx, imgh);
      }
      for (let yi=layer.h, dsty=this.maph+0.5; yi-->0; dsty+=this.maph+1) {
        ctx.moveTo(0, dsty);
        ctx.lineTo(imgw, dsty);
      }
      ctx.strokeStyle = "#222";
      ctx.stroke();
    }
  }
  
  drawMap(img, ctx, dstx, dsty, map) {
    const imageName = map.cmd.getFirstArg("image");
    const imageRes = this.data.findResource(imageName, "image");
    const ctab = this.ctabv[imageRes.rid];
    for (let yi=this.maph, y=dsty, p=0; yi-->0; y++) {
      for (let xi=this.mapw, x=dstx; xi-->0; x++, p++) {
        const tileid = map.v[p];
        ctx.fillStyle = ctab ? ctab[tileid] : `rgb(${tileid},${tileid},${tileid})`;
        ctx.fillRect(x, y, 1, 1);
      }
    }
  }
}
