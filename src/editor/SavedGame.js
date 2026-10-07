/* SavedGame.js
 * Model for encoding and decoding saved games.
 * Writing this for the new compact format, 2026-10-07. Before, it was all piecemeal in EditSaveModal.js.
 * Format is described at src/game/store.h.
 */
 
// We could get these from SharedSymbols, but then we'd need that injected. Whatever. They're not going to change.
const MAPW = 20;
const MAPH = 12;
 
export class SavedGame {

  /* (serial) should be a string of base64.
   * Empty or null is fine, but otherwise we throw if malformed.
   * We do assert the checksum and everything.
   * Jigstore will initially be packed. You'll want to unpack before any model operations and pack again before encoding.
   */
  constructor(serial) {
    this.fld = []; // 0|1
    this.fld16 = []; // 0..0xffff
    this.clock = []; // 0..0x3fffffff, ms
    this.jigstore = []; // {mapid,x,y,xform} | {sequentialc,names}
    this.invstore = []; // {itemid,limit,quantity}
    if (serial) this._decode(serial);
  }
  
  /* (jigstore) can have marker entries that indicate repetition of the previous entry, as encoded.
   * Or they can be unpacked into the flat runtime list.
   * Packing or unpacking requires knowledge of the data set, so you must give us an instance of MapService.
   * Both packed and unpacked forms are legal for encoding, but you definitely want to pack before encoding.
   * The logic here is massively inefficient. It works much better at runtime, where the objects are immutable and better indexed.
   ***************************************************************************************/
   
  sortJigstore() {
    // Operate only if there are no repeats. Only unpacked jigstore should get sorted.
    if (!this.jigstore.find(j => !j.mapid)) {
      this.jigstore.sort((a, b) => a.mapid - b.mapid);
    }
    return this;
  }
  
  packJigstore(mapService) {
    /* Same as the game currently, we're not going to produced name lists.
     * When first devising the format, I assumed those would be a big win, but in practice they seem to actually degrade compression.
     * But I'm not convinced that that's the end of the story, so they do exist (eg we decode them properly if present).
     *
     * We will only pack sequential jigstores when they are sequential in our storage.
     * So callers are strongly advised to sort before packing.
     */
    for (let p=0; p<this.jigstore.length; ) {
      const explicit = this.jigstore[p++];
      let sequentialc = 0;
      while (
        (p + sequentialc < this.jigstore.length) &&
        (this.jigstore[p + sequentialc].mapid === explicit.mapid + sequentialc + 1) &&
        this.jigstoreIsInExpectedPlace(mapService, explicit, this.jigstore[p + sequentialc])
      ) {
        sequentialc++;
      }
      if (!sequentialc) continue;
      this.jigstore.splice(p, sequentialc, { sequentialc, names: [] });
      p++;
    }
    return this;
  }
  
  unpackJigstore(mapService) {
    for (let p=0; p<this.jigstore.length; ) {
      const explicit = this.jigstore[p++];
      if ((p < this.jigstore.length) && !this.jigstore[p].mapid) {
        const repeat = this.jigstore[p];
        this.jigstore.splice(p, 1);
        if (repeat.sequentialc) {
          for (let i=repeat.sequentialc, mapid=explicit.mapid+1; i-->0; mapid++) {
            const [x, y] = this.expectedPlaceForJigstore(mapService, explicit, mapid);
            this.jigstore.splice(p, 0, { mapid, x, y, xform: explicit.xform });
            p++;
          }
        }
        if (repeat.names.length) {
          for (const mapid of repeat.names) {
            const [x, y] = this.expectedPlaceForJigstore(mapService, explicit, mapid);
            this.jigstore.splice(p, 0, { mapid, x, y, xform: explicit.xform });
            p++;
          }
        }
      }
    }
    return this;
  }
  
  jigstoreIsInExpectedPlace(mapService, ref, jigstore) {
    if (!ref?.mapid || !jigstore?.mapid) return false;
    if (ref.xform !== jigstore.xform) return false;
    for (const plane of mapService.layout) {
      const p = plane.v.findIndex(q => q?.rid === ref.mapid);
      if (p < 0) continue;
      const lng = p % plane.w;
      const lat = Math.floor(p / plane.w);
      
      const jp = plane.v.findIndex(q => q?.rid === jigstore.mapid);
      if (jp < 0) return; // (ref) and (jigstore) are not on the same plane.
      const jlng = jp % plane.w;
      const jlat = Math.floor(jp / plane.w);
      
      const dlng = jlng - lng;
      const dlat = jlat - lat;
      let expectx=ref.x, expecty=ref.y;
      switch (ref.xform) {
        case 0: expectx += dlng * MAPW; expecty += dlat * MAPH; break;
        case 6: expectx -= dlat * MAPH; expecty += dlng * MAPW; break;
        case 3: expectx -= dlng * MAPW; expecty -= dlat * MAPH; break;
        case 5: expectx += dlat * MAPH; expecty -= dlng * MAPW; break;
        default: return false;
      }
      if ((expectx !== jigstore.x) || (expecty !== jigstore.y)) return false;
      return true;
    }
    return false;
  }
  
  // Returns [x,y].
  expectedPlaceForJigstore(mapService, ref, mapid) {
    for (const plane of mapService.layout) {
      const p = plane.v.findIndex(q => q?.rid === ref.mapid);
      if (p < 0) continue;
      const lng = p % plane.w;
      const lat = Math.floor(p / plane.w);
      
      const jp = plane.v.findIndex(q => q?.rid === mapid);
      if (jp < 0) return; // (ref) and (mapid) are not on the same plane.
      const jlng = jp % plane.w;
      const jlat = Math.floor(jp / plane.w);
      
      const dlng = jlng - lng;
      const dlat = jlat - lat;
      let expectx=ref.x, expecty=ref.y;
      switch (ref.xform) {
        case 0: expectx += dlng * MAPW; expecty += dlat * MAPH; break;
        case 6: expectx -= dlat * MAPH; expecty += dlng * MAPW; break;
        case 3: expectx -= dlng * MAPW; expecty -= dlat * MAPH; break;
        case 5: expectx += dlat * MAPH; expecty -= dlng * MAPW; break;
      }
      return [expectx, expecty];
    }
    throw new Error(`maps ${ref.mapid} and ${mapid} are not on the same plane`);
  }
  
  /* Encode to a base64 string.
   **********************************************************************************/
  
  encode() {
    let dst = "/";
    dst += this.encodeHeap(this.encodeFldv());
    dst += this.encodeHeap(this.encodeFld16v());
    dst += this.encodeHeap(this.encodeClockv());
    dst += this.encodeHeap(this.encodeJigstorev());
    dst += this.encodeHeap(this.encodeInvstorev());
    dst += this.encodeScalar(this.checksum(dst, dst.length), 5);
    return dst;
  }
  
  encodeHeap(src) {
    if (src.length > 0xfff) throw new Error(`Heap too large (${src.length}>4095)`);
    return this.encodeScalar(src.length, 2) + src;
  }
  
  encodeScalar(src, len) {
    if (src < 0) throw new Error(`Unexpected negative scalar ${src}`);
    const src0 = src;
    let dst = "";
    for (; len-->0; src>>=6) dst = this.base64digit(src & 0x3f) + dst;
    if (src) throw new Error(`Input ${src0} too long for ${len}-byte scalar`);
    return dst;
  }
  
  base64digit(src) {
    if ((src < 0) || (src > 0x3f)) throw new Error(`Invalid base64 scalar ${src}`);
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[src];
  }
  
  /* Decode from a base64 string.
   * Only called internally, at construction.
   **********************************************************************************/
  
  _decode(serial) {
    
    // Validate signature.
    if (!serial) return; // Empty is fine.
    if (!serial.startsWith("/")) throw new Error("Invalid signature");
    
    // Decode each heap.
    let serialp = 1;
    serialp = this.decodeHeap(serial, serialp, h => this.decodeFldv(h));
    serialp = this.decodeHeap(serial, serialp, h => this.decodeFld16v(h));
    serialp = this.decodeHeap(serial, serialp, h => this.decodeClockv(h));
    serialp = this.decodeHeap(serial, serialp, h => this.decodeJigstorev(h));
    serialp = this.decodeHeap(serial, serialp, h => this.decodeInvstorev(h));
    
    // Validate checksum.
    if (serialp !== serial.length - 5) throw new Error(`Checksum length mismatch @${serialp}/${serial.length}`);
    const actual = this.checksum(serial, serialp);
    const expect = this.readScalar(serial, serialp, 5);
    if (actual !== expect) throw new Error(`Checksum failure. expect=${expect} actual=${actual}`);
  }
  
  // Read length and call (cb) with the encoded payload. Returns new (srcp).
  decodeHeap(src, srcp, cb) {
    if (srcp > src.length - 2) throw new Error("Unexpected EOF");
    const len = this.readScalar(src, srcp, 2);
    srcp += 2;
    if (srcp > src.length - len) throw new Error(`Unexpected EOF, expecting ${len}-byte chunk at ${srcp}/${src.length}`);
    cb(src.substring(srcp, srcp + len));
    return srcp + len;
  }
  
  readScalar(src, srcp, len) {
    if ((srcp < 0) || (len < 1) || (srcp > src.length - len)) throw new Error("Invalid scalar geometry");
    let dst = 0;
    for (; len-->0; srcp++) {
      dst <<= 6;
      dst |= this.decodeDigit(src.charCodeAt(srcp));
    }
    return dst;
  }
  
  decodeDigit(v) {
    if (typeof(v) === "string") v = v.charCodeAt(0);
    if ((v >= 0x41) && (v <= 0x5a)) return v - 0x41;
    if ((v >= 0x61) && (v <= 0x7a)) return v - 0x61 + 26;
    if ((v >= 0x30) && (v <= 0x39)) return v - 0x30 + 52;
    if (v === 0x2b) return 62;
    if (v === 0x2f) return 63;
    throw new Error(`Invalid base64 digit ${v}`);
  }
  
  checksum(src, len) {
    let sum = 0;
    for (let i=0; i<len; i++) {
      sum = (sum >>> 31) | (sum << 1);
      sum ^= src.charCodeAt(i);
    }
    return sum & 0x3fffffff;
  }
  
  /* Encode and decode for the five heaps.
   ***********************************************************************************/
   
  encodeFldv() {
    let dst = "";
    let dstbuf = 0;
    let dstshift = 0;
    let pv = 0;
    let runlen = 0;
    const emit = () => {
      dstbuf |= runlen << dstshift;
      if (dstshift) {
        dstshift = 0;
        dst += this.base64digit(dstbuf);
        dstbuf = 0;
      } else {
        dstshift = 3;
      }
    };
    if (this.fld.length) this.fld[0] = 0; // It's an encoding requirement that [0] be zero. Force it if not.
    for (const v of this.fld) {
      if (v === pv) {
        runlen++;
        if (runlen >= 7) {
          emit();
          runlen -= 7;
        }
      } else {
        emit();
        pv = v;
        runlen = 1;
      }
    }
    if (runlen) emit();
    if (dstbuf) dst += this.base64digit(dstbuf);
    return dst;
  }
  
  decodeFldv(src) {
    for (let srcp=0, v=0; srcp<src.length; srcp++) {
      const intake = this.decodeDigit(src.charCodeAt(srcp));
      let runlen = intake & 7;
      for (let i=0; i<runlen; i++) this.fld.push(v);
      if (runlen !== 7) v ^= 1;
      runlen = intake >> 3;
      for (let i=0; i<runlen; i++) this.fld.push(v);
      if (runlen !== 7) v ^= 1;
    }
  }
  
  encodeFld16v() {
    let dst = "";
    for (const v of this.fld16) {
      if (v < 0x0020) {
        dst += this.base64digit(v);
      } else if (v < 0x0400) {
        dst += this.base64digit(0x20 | (v >> 5));
        dst += this.base64digit(v & 0x1f);
      } else {
        dst += this.base64digit(0x20 | (v >> 11));
        dst += this.base64digit(0x20 | ((v >> 6) & 0x1f));
        dst += this.base64digit(v & 0x3f);
      }
    }
    return dst;
  }
  
  decodeFld16v(src) {
    for (let srcp=0; srcp<src.length; ) {
      let v = 0;
      let intake = this.decodeDigit(src[srcp++]);
      if (intake & 0x20) {
        v = (intake & 0x1f) << 5;
        intake = this.decodeDigit(src[srcp++]);
        if (intake & 0x20) {
          v <<= 6;
          v |= (intake & 0x1f) << 6;
          intake = this.decodeDigit(src[srcp++]);
          v |= intake;
        } else {
          v |= intake;
        }
      } else {
        v = intake;
      }
      this.fld16.push(v);
    }
  }
  
  encodeClockv() {
    let dst = "";
    for (const v of this.clock) {
      dst += this.encodeScalar(v, 5);
    }
    return dst;
  }
  
  decodeClockv(src) {
    for (let srcp=0; srcp<src.length; srcp+=5) {
      this.clock.push(this.readScalar(src, srcp, 5));
    }
  }
  
  encodeJigstorev() {
    // We may be packed or unpacked; both forms are encodable. But it's wise for caller to pack first.
    let dst = "";
    for (const jigstore of this.jigstore) {
      if (jigstore.mapid) {
        const v = (jigstore.mapid << 19) | (jigstore.x << 11) | (jigstore.y << 3) | jigstore.xform;
        dst += this.encodeScalar(v, 5);
      } else if (jigstore.sequentialc || jigstore.names.length) {
        const v = (jigstore.sequentialc << 7) | jigstore.names.length;
        dst += this.encodeScalar(v, 5);
        for (const name of jigstore.names) {
          dst += this.encodeScalar(name, 2);
        }
      }
    }
    return dst;
  }
  
  decodeJigstorev(src) {
    for (let srcp=0; srcp<src.length; ) {
      const raw = this.readScalar(src, srcp, 5);
      srcp += 5;
      if (raw & 0xfff80000) { 
        const mapid = raw >> 19;
        const x = (raw >> 11) & 0xff;
        const y = (raw >> 3) & 0xff;
        const xform = raw & 7;
        this.jigstore.push({ mapid, x, y, xform });
      } else {
        const sequentialc = (raw >> 7) & 0x7f;
        const namedc = raw & 0x7f;
        if (!sequentialc && !namedc) continue; // I'll grudgingly allow that a straight zero is legal.
        if (!this.jigstore[this.jigstore.length-1]?.mapid) throw new Error(`repeat jigstore was not preceded by an explicit one`);
        const names = [];
        for (let i=0; i<namedc; i++) {
          names.push(this.readScalar(src, srcp, 2));
          srcp += 2;
        }
        this.jigstore.push({ sequentialc, names });
      }
    }
  }
  
  encodeInvstorev() {
    let dst = "";
    let reqc = this.invstore.length;
    while (reqc && !this.invstore[reqc-1].itemid) reqc--;
    for (let i=0; i<reqc; i++) {
      const invstore = this.invstore[i];
      if ((invstore.itemid < 63) && !invstore.limit && !invstore.quantity) {
        dst += this.base64digit(invstore.itemid);
      } else {
        dst += "/";
        dst += this.encodeScalar((invstore.itemid << 16) | (invstore.limit << 8) | invstore.quantity, 4);
      }
    }
    return dst;
  }
  
  decodeInvstorev(src) {
    for (let srcp=0; srcp<src.length; ) {
      let v = this.decodeDigit(src.charCodeAt(srcp++));
      if (v === 63) { // Long form.
        v = this.readScalar(src, srcp, 4);
        srcp += 4;
        const itemid = v >> 16;
        const limit = (v >> 8) & 0xff;
        const quantity = v & 0xff;
        this.invstore.push({ itemid, limit, quantity });
      } else { // Short form, itemid only.
        this.invstore.push({ itemid: v, limit: 0, quantity: 0 });
      }
    }
  }
}
