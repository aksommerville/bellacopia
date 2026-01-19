import { Dom } from "./js/Dom.js";

export class Rgb332Modal {
  static getDependencies() {
    return [HTMLDialogElement, Dom];
  }
  constructor(element, dom) { 
    this.element = element;
    this.dom = dom;
    
    this.result = new Promise((resolve, reject) => {
      this.resolve = resolve;
      this.reject = reject;
    });
  }
  
  onRemoveFromDom() {
    this.resolve(null);
  }
  
  setup(src) {
    let r=0, g=0, b=0;
    if (typeof(src) === "string") {
      if (src.startsWith("#") && (src.length === 7)) {
        r = parseInt(src.substring(1, 3), 16) || 0; r >>= 5;
        g = parseInt(src.substring(3, 5), 16) || 0; g >>= 5;
        b = parseInt(src.substring(5, 7), 16) || 0; b >>= 6;
      }
    }
    const rslider = this.dom.spawn(this.element, "INPUT", { type: "range", name: "r", min: 0, max: 7, value: r, "on-input": () => this.onInput() });
    const gslider = this.dom.spawn(this.element, "INPUT", { type: "range", name: "g", min: 0, max: 7, value: g, "on-input": () => this.onInput() });
    const bslider = this.dom.spawn(this.element, "INPUT", { type: "range", name: "b", min: 0, max: 3, value: b, "on-input": () => this.onInput() });
    this.dom.spawn(this.element, "DIV", ["preview"]);
    this.dom.spawn(this.element, "INPUT", { type: "submit", value: "OK", "on-click": () => this.onSubmit() });
    this.onInput(); // Initial preview
  }
  
  onInput() {
    let r = +this.element.querySelector("input[name='r']").value << 5; r |= r >> 3; r |= r >> 6;
    let g = +this.element.querySelector("input[name='g']").value << 5; g |= g >> 3; g |= g >> 6;
    let b = +this.element.querySelector("input[name='b']").value; b |= b << 2; b |= b << 4;
    this.element.querySelector(".preview").style.backgroundColor = "#" + r.toString(16).padStart(2, '0') + g.toString(16).padStart(2, '0') + b.toString(16).padStart(2, '0');
  }
  
  onSubmit() {
    const r = +this.element.querySelector("input[name='r']").value;
    const g = +this.element.querySelector("input[name='g']").value;
    const b = +this.element.querySelector("input[name='b']").value;
    this.resolve((r << 5) | (g << 2) | b);
    this.element.remove();
  }
}
