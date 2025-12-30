/* store.h
 * Serial data dump.
 * Our data is exposed as enumerated fields.
 * Field ID is the bitwise index, and you tell us the size on each access.
 * Size is arbitrarily limited to 16 bits.
 */
 
#ifndef STORE_H
#define STORE_H

/* The general store.
 *****************************************************************************/

void store_reset();

int store_get(int fld,int size);
int store_set(int fld,int size,int v); // => nonzero if changed

/* You may listen on (0,0) to be notified of all changes.
 * Otherwise (fld,size) must be a valid field.
 * It's safe to unlisten during your own callback, but it is *not* safe to unlisten anyone else then.
 * All listeners get dropped at reset.
 */
int store_listen(int fld,int size,void (*cb)(int fld,int size,int v,void *userdata),void *userdata); // => listenerid>0
void store_unlisten(int listenerid);
void store_unlisten_all();

/* Loading resets (ie all listeners get dropped).
 */
int store_load();
int store_save();
int store_save_if_dirty();

/* Jigsaw puzzle progress goes in a separate store, because it is bulky and highly structured.
 ****************************************************************************************/
 
void store_jigsaw_load();
void store_jigsaw_save_if_dirty(int immediate); // (immediate) nonzero to skip the debounce.

int store_jigsaw_get(int *x,int *y,uint8_t *xform,int mapid);
int store_jigsaw_set(int mapid,int x,int y,uint8_t xform);

#endif
