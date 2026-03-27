/* EditSaveInventoryModal.js
 */
 
import { Dom } from "../js/Dom.js";
import { SharedSymbols } from "../js/SharedSymbols.js";

export class EditSaveInventoryModal {
  static getDependencies() {
    return [HTMLDialogElement, Dom, SharedSymbols, Window, "nonce"];
  }
  constructor(element, dom, sharedSymbols, window, nonce) {
    this.element = element;
    this.dom = dom;
    this.sharedSymbols = sharedSymbols;
    this.window = window;
    this.nonce = nonce;
    
    this.itemidListId = `ESIM-${this.nonce}-itemid-list`;
    this.itemNames = []; // Indexed by itemid.
    
    // Resolve with null if cancelled or unchanged. Otherwise with the new JSON text.
    this.model = []; // {itemid,limit,quantity}, all in 0..255.
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(src) {
    this.model = JSON.parse(src || "[]");
    this.buildUi();
  }
  
  buildUi() {
    this.element.innerHTML = "";
    
    this.itemNames = [];
    const list = this.dom.spawn(this.element, "DATALIST", { id: this.itemidListId });
    for (const sym of this.sharedSymbols.symv) {
      if (sym.ns !== "itemid") continue;
      if (sym.nstype !== "NS") continue;
      this.dom.spawn(list, "OPTION", /*{ value: sym.v },*/ sym.k);
      this.itemNames[sym.v] = sym.k;
    }
    
    /* Mimic the runtime's inventory vellum: 5x5 grid plus one on the side.
     * [0] is the one on the side, and [1..25] are placed LRTB in the grid.
     * A lot of things are dependent on metadata from src/game/inventory.c.
     * In theory, we could read that and present something user-friendlier.
     * But I don't imagine it's worth the effort. Anyway, we might want to deliberately produce invalid saved games.
     * One table with 6 columns; the rightmost column is only used by the top row.
     */
    const grid = this.dom.spawn(this.element, "TABLE", ["grid"]);
    const addRow = (p) => {
      const tr = this.dom.spawn(grid, "TR");
      if (!p) p = 1;
      for (let i=0; i<5; i++) {
        const td = this.dom.spawn(tr, "TD", ["cell"], { "data-index": p + i });
        this.populateCell(td, this.model[p + i]);
      }
      if (p === 1) {
        const td = this.dom.spawn(tr, "TD", ["cell"], { "data-index": 0 });
        this.populateCell(td, this.model[0]);
      } else if (p === 21) { // Add an OK button at the bottom row.
        this.dom.spawn(tr, "TD",
          this.dom.spawn(null, "INPUT", { type: "button", value: "OK", "on-click": () => this.onSubmit() })
        );
      }
    };
    addRow(0);
    addRow(6);
    addRow(11);
    addRow(16);
    addRow(21);
  }
  
  populateCell(parent, inv) {
    parent.innerHTML = "";
    const itemid = this.itemidRepr(inv?.itemid);
    const limit = inv?.limit || "";
    const quantity = inv?.quantity || "";
    const table = this.dom.spawn(parent, "TABLE");
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], "itemid"),
      this.dom.spawn(null, "TD", ["value"],
        this.dom.spawn(null, "INPUT", { type: "text", name: "itemid", list: this.itemidListId, value: itemid })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], "limit"),
      this.dom.spawn(null, "TD", ["value"],
        this.dom.spawn(null, "INPUT", { type: "number", name: "limit", min: 0, max: 255, value: limit })
      )
    );
    this.dom.spawn(table, "TR",
      this.dom.spawn(null, "TD", ["key"], "quantity"),
      this.dom.spawn(null, "TD", ["value"],
        this.dom.spawn(null, "INPUT", { type: "number", name: "quantity", min: 0, max: 255, value: quantity })
      )
    );
  }
  
  itemidRepr(src) {
    if (typeof(src) === "string") src = +src.trim() || 0;
    if (!src) return "";
    return this.itemNames[src] || src.toString();
  }
  
  itemidEval(src) {
    if (!src) return 0;
    const n = +src?.trim?.();
    if (n || (n === 0)) return n;
    const p = this.itemNames.indexOf(src);
    if (p >= 0) return p;
    return 0;
  }
  
  modelFromUi() {
    const model = [];
    for (let index=0; index<26; index++) {
      const inv = { itemid: 0, limit: 0, quantity: 0 };
      const td = this.element.querySelector(`td.cell[data-index='${index}']`);
      if (td) {
        inv.itemid = this.itemidEval(td.querySelector("input[name='itemid']").value);
        inv.limit = +td.querySelector("input[name='limit']").value || 0;
        inv.quantity = +td.querySelector("input[name='quantity']").value || 0;
      }
      model.push(inv);
    }
    return model;
  }
  
  onSubmit() {
    this.resolve(JSON.stringify(this.modelFromUi()));
    this.element.remove();
  }
}
