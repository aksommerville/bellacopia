/* EditWorldModal.js
 * Shows one plane, similar to the generic "World Map", but also lets you edit Bellacopia-specific things at that scope.
 * You pick which attribute you want, and we show a colored box over each map that has a value for it.
 * Super helpful for detecting missing song or rsprite.
 */
 
import { Dom } from "../js/Dom.js";
import { Data } from "../js/Data.js";
import { MapService } from "../js/map/MapService.js";

const HIGHLIGHT_COLORS = [
  "#f008",
  "#0f08",
  "#00f8",
  "#ff08",
  "#f0f8",
  "#0ff8",
  "#f808",
  "#f088",
  "#8f08",
  "#80f8",
  "#0f88",
  "#08f8",
];

export class EditWorldModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, Data, Window, MapService];
  }
  constructor(element, dom, data, window, mapService) {
    this.element = element;
    this.dom = dom;
    this.data = data;
    this.window = window;
    this.mapService = mapService;
    
    this.renderTimeout = null;
    this.layers = this.mapService.layout.map(v => v.crop());
    this.layer = null;
    this.mapw = 1; // Render geometry only reliable if (this.layer) set.
    this.maph = 1; // Map size in canvas pixels. Includes a wee margin.
    this.tilesize = 0; // Zero to recalculate at next render.
    this.renderx = 0; // top left of plane in canvas pixels.
    this.rendery = 0;
    this.tattleMap = null;
    
    // Load all images.
    const promises = [];
    this.imageByRid = []; // Promise<Image> or Image.
    for (const layer of this.layers) {
      for (const res of layer.v) {
        if (!res?.map) continue;
        const map = res.map;
        const prerid = map.cmd.getFirstArg("image");
        if (!prerid) continue;
        const ires = this.data.findResource(prerid, "image");
        if (!ires) continue;
        if (this.imageByRid[ires.rid]) continue; // Already loading, cool.
        promises.push(this.imageByRid[ires.rid] = this.data.getImageAsync(ires.rid).then(image => {
          this.imageByRid[ires.rid] = image;
        }).catch(e => {
          this.imageByRid[ires.rid] = "error";
          console.error(e);
        }));
      }
    }
    Promise.all(promises).then(() => this.renderSoon());
    
    this.buildUi();
  }
  
  onRemoveFromDom() {
    if (this.renderTimeout) {
      this.window.clearTimeout(this.renderTimeout);
    }
  }
  
  /* UI.
   ***********************************************************************/
  
  buildUi() {
    this.element.innerHTML = "";
    const topRow = this.dom.spawn(this.element, "DIV", ["topRow"]);
    
    const layerSelect = this.dom.spawn(topRow, "SELECT", { name: "layer", "on-change": e => this.onLayerChange(e) });
    this.dom.spawn(layerSelect, "OPTION", { disabled: "disabled", value: "none" }, "Layer...");
    for (const layer of this.layers) {
      this.dom.spawn(layerSelect, "OPTION", { value: layer.z }, this.reprLayer(layer));
    }
    layerSelect.value = "none";
    
    this.dom.spawn(topRow, "SELECT", { name: "bg", value: "black", "on-change": () => this.renderSoon() },
      this.dom.spawn(null, "OPTION", { value: "black" }, "bg black"),
      this.dom.spawn(null, "OPTION", { value: "gray" }, "bg gray"),
      this.dom.spawn(null, "OPTION", { value: "white" }, "bg white"),
    );
    
    this.dom.spawn(topRow, "SELECT", { name: "op", value: "rsprite", "on-change": () => { this.refreshTattle(); this.renderSoon(); } },
      this.dom.spawn(null, "OPTION", { value: "rsprite" }, "rsprite"),
      this.dom.spawn(null, "OPTION", { value: "song" }, "song"),
      this.dom.spawn(null, "OPTION", { value: "parent" }, "parent"),
    );
    
    this.dom.spawn(topRow, "DIV", ["spacer"]);
    this.dom.spawn(topRow, "DIV", ["tattle"]);
    
    this.dom.spawn(this.element, "CANVAS", ["visual"], {
      "on-click": e => this.onVisualClick(e),
      "on-mousemove": e => this.onMotion(e),
      "on-mouseleave": e => this.setTattle(null),
    });
  }
  
  reprLayer(layer) {
    return `${layer.z} (${layer.w}x${layer.h})`;
  }
  
  reprRspriteFromMap(map) {
    if (!map) return "";
    let result = "";
    for (const cmd of map.cmd.commands) {
      if (cmd[0] !== "rsprite") continue;
      if (result) return "MULTIPLE";
      result = cmd[2] || "INVALID";
    }
    return result;
  }
  
  setTattle(map) {
    if (map === this.tattleMap) return;
    this.tattleMap = map;
    this.refreshTattle();
  }
  
  refreshTattle() {
    if (this.tattleMap) {
      const op = this.element.querySelector("select[name='op']")?.value;
      let addl = "";
      switch (op) {
        case "rsprite": addl = this.reprRspriteFromMap(this.tattleMap); break;
        case "song": addl = this.tattleMap.cmd.getFirstArg("song") || ""; break;
        case "parent": addl = this.tattleMap.cmd.getFirstArg("parent") || ""; break;
      }
      this.element.querySelector(".tattle").innerText = `${addl} map:${this.tattleMap.rid}`;
    } else {
      this.element.querySelector(".tattle").innerText = "";
    }
  }
  
  /* Render.
   **************************************************************************/
   
  renderSoon() {
    if (this.renderTimeout) return;
    this.renderTimeout = this.window.setTimeout(() => {
      this.renderTimeout = null;
      this.renderNow();
    }, 50);
  }
  
  renderNow() {
    const canvas = this.element.querySelector(".visual");
    const bounds = canvas.getBoundingClientRect();
    canvas.width = bounds.width;
    canvas.height = bounds.height;
    const ctx = canvas.getContext("2d");
    
    switch (this.element.querySelector("select[name='bg']")?.value) {
      case "black": ctx.fillStyle = "#000"; break;
      case "gray": ctx.fillStyle = "#888"; break;
      case "white": ctx.fillStyle = "#fff"; break;
    }
    ctx.fillRect(0, 0, bounds.width, bounds.height);
    if (!this.layer) return;
    
    const op = this.element.querySelector("select[name='op']")?.value;
    
    if (!this.tilesize) {
      const res = this.layer.v.find(v => v); // Any map. Assume they're all the same size.
      if (!res) return;
      const map = res.map;
      const spacing = 2;
      const worldwm = map.w * this.layer.w;
      const worldhm = map.h * this.layer.h;
      const xtotalspace = (this.layer.w - 1) * spacing;
      const ytotalspace = (this.layer.h - 1) * spacing;
      const xscale = (bounds.width - xtotalspace) / worldwm;
      const yscale = (bounds.height - ytotalspace) / worldhm;
      const scale = Math.max(1, Math.floor(Math.min(xscale, yscale)));
      this.tilesize = scale;
      this.mapw = map.w * this.tilesize + spacing;
      this.maph = map.h * this.tilesize + spacing;
      this.renderx = (canvas.width >> 1) - ((this.mapw * this.layer.w) >> 1);
      this.rendery = (canvas.height >> 1) - ((this.maph * this.layer.h) >> 1);
    }
    
    this.highlightValues = [];
    for (let dsty=this.rendery, mapp=0, yi=this.layer.h; yi-->0; dsty+=this.maph) {
      for (let dstx=this.renderx, xi=this.layer.w; xi-->0; dstx+=this.mapw, mapp++) {
        const map = this.layer.v[mapp]?.map;
        if (!map) continue;
        this.renderMap(ctx, dstx, dsty, map, op);
      }
    }
  }
  
  renderMap(ctx, dstx, dsty, map, op) {
    const prerid = map.cmd.getFirstArg("image");
    const ires = this.data.findResource(prerid, "image");
    if (!ires) return;
    const image = this.imageByRid[ires.rid];
    if (!image) return;
    if (!(image instanceof Image)) return;
    const srctilesize = image.naturalWidth >> 4;
    for (let suby=dsty, yi=map.h, p=0; yi-->0; suby+=this.tilesize) {
      for (let subx=dstx, xi=map.w; xi-->0; subx+=this.tilesize, p++) {
        const tileid = map.v[p];
        const srcx = (tileid & 15) * srctilesize;
        const srcy = (tileid >> 4) * srctilesize;
        ctx.drawImage(image, srcx, srcy, srctilesize, srctilesize, subx, suby, this.tilesize, this.tilesize);
      }
    }
    let highlightValue = "";
    switch (op) {
      case "rsprite": highlightValue = this.reprRspriteFromMap(map); break;
      case "song": highlightValue = map.cmd.getFirstArg("song"); break;
      case "parent": highlightValue = map.cmd.getFirstArg("parent"); break;
    }
    if (highlightValue) {
      let p = this.highlightValues.indexOf(highlightValue);
      if (p < 0) {
        p = this.highlightValues.length;
        this.highlightValues.push(highlightValue);
      }
      ctx.fillStyle = "#0008"; // First 50% black, to ensure the highlight shows clearly.
      ctx.fillRect(dstx, dsty, this.mapw, this.maph);
      ctx.fillStyle = HIGHLIGHT_COLORS[p % HIGHLIGHT_COLORS.length];
      ctx.fillRect(dstx, dsty, this.mapw >> 1, this.maph >> 1);
    }
  }
  
  /* Model.
   **************************************************************************/
   
  collectOptions(map, op) {
    switch (op) {
    
      case "rsprite": {
          // Show options that have been used as "rsprite" in other maps on this plane.
          // Could use all planes, or all sprites, or sprites with "monster" type, but any of those would be too many.
          // In fact even this approach might be a bit much.
          const options = ["OTHER"];
          for (const res of this.layer.v) {
            const map = res?.map;
            if (!map) continue;
            const option = this.reprRspriteFromMap(map);
            if (!option || (option === "MULTIPLE") || (option === "INVALID")) continue;
            if (options.indexOf(option) >= 0) continue;
            options.push(option);
          }
          return options;
        }
        
      case "song": {
          // Show all songs in the TOC. It shouldn't be too unwieldly a set.
          return this.data.resv.filter(r => r.type === "song").map(r => `song:${r.name || r.rid}`);
        }
        
      case "parent": {
          // Show all parents used by maps in this plane.
          // All maps is too many, and all maps with a door is probably also too many.
          const options = ["OTHER"];
          for (const res of this.layer.v) {
            const map = res?.map;
            if (!map) continue;
            const option = map.cmd.getFirstArg("parent");
            if (!option) continue;
            if (options.indexOf(option) >= 0) continue;
            options.push(option);
          }
          return options;
        }
    }
    return [];
  }
  
  // Return true or Promise<boolean> if we modify (map). Caller syncs and rerenders.
  applyOption(map, op, option) {
    switch (op) {
    
      case "rsprite": {
          if (option === "OTHER") {
            return this.dom.modalText("Sprite name or ID:", "").then(rsp => {
              if (!rsp) return false;
              const cmd = map.cmd.commands.find(c => c[0] === "rsprite");
              if (cmd) cmd[2] = rsp;
              else map.cmd.commands.push(["rsprite", "(u16)0", `sprite:${rsp}`, "(u32)0"]);
              return true;
            });
          }
          const cmd = map.cmd.commands.find(c => c[0] === "rsprite");
          if (cmd) {
            if (cmd[2] === option) return false;
            cmd[2] = option;
          } else {
            map.cmd.commands.push(["rsprite", "(u16)0", option, "(u32)0"]);
          }
        } return true;
        
      case "song": {
          const cmd = map.cmd.commands.find(c => c[0] === "song");
          if (cmd) {
            if (cmd[1] === option) return false;
            cmd[1] = option;
          } else {
            map.cmd.commands.push(["song", option]);
          }
        } return true;
        
      case "parent": {
          if (option === "OTHER") {
            return this.dom.modalText("Map name or ID:", "").then(rsp => {
              if (!rsp) return false;
              const cmd = map.cmd.commands.find(c => c[0] === "parent");
              if (cmd) cmd[1] = `map:${rsp}`;
              else map.cmd.commands.push(["parent", `map:${rsp}`]);
              return true;
            });
          }
          const cmd = map.cmd.commands.find(c => c[0] === "parent");
          if (cmd) {
            if (cmd[1] === option) return false;
            cmd[1] = option;
          } else {
            map.cmd.commands.push(["parent", option]);
          }
        } return true;
    }
    return false;
  }
  
  /* Events.
   ***************************************************************************/
  
  onLayerChange(event) {
    const z = +event.target.value;
    this.layer = this.layers.find(l => l.z === z); // null is ok
    this.tilesize = 0;
    this.renderSoon();
  }
  
  onVisualClick(event) {
    if (this.tilesize < 1) return;
    if (!this.layer) return;
    const bounds = event.target.getBoundingClientRect();
    const x = event.clientX - bounds.x;
    const y = event.clientY - bounds.y;
    const col = Math.floor((x - this.renderx) / this.mapw);
    const row = Math.floor((y - this.rendery) / this.maph);
    if ((col < 0) || (row < 0) || (col >= this.layer.w) || (row >= this.layer.h)) return;
    const res = this.layer.v[row * this.layer.w + col];
    const map = res?.map;
    if (!map) return;
    const op = this.element.querySelector("select[name='op']")?.value;
    const options = this.collectOptions(map, op);
    if (options.length < 1) return;
    this.dom.modalPickOne(`${op} for map:${map.rid}:`, options).then(option => {
      if (!option) return;
      let result = this.applyOption(map, op, option);
      if (!result) return;
      if (!(result instanceof Promise)) result = Promise.resolve(result);
      result.then(dirty => {
        if (!dirty) return;
        this.data.dirty(res.path, () => map.encode());
        this.renderSoon();
      });
    }).catch(e => this.dom.modalError(e));
  }
  
  onMotion(event) {
    if (this.tilesize < 1) return;
    if (!this.layer) return;
    const bounds = event.target.getBoundingClientRect();
    const x = event.clientX - bounds.x;
    const y = event.clientY - bounds.y;
    const col = Math.floor((x - this.renderx) / this.mapw);
    const row = Math.floor((y - this.rendery) / this.maph);
    let map = null;
    if ((col >= 0) && (row >= 0) && (col < this.layer.w) && (row < this.layer.h)) {
      map = this.layer.v[row * this.layer.w + col]?.map;
    }
    this.setTattle(map);
  }
}
