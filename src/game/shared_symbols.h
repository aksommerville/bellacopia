#ifndef SHARED_SYMBOLS_H
#define SHARED_SYMBOLS_H

#define EGGDEV_importUtil "stdlib,res,graf,font,text"

#define NS_sys_tilesize 16
#define NS_sys_mapw 20 /* Maps must be at least as large as the framebuffer, not a pixel less. */
#define NS_sys_maph 12 /* Because map renderer will assume a 2x2 set of neighbor maps is all you can ever see. */

#define CMD_map_dark       0x01 /* --- ; Lights out initially, matches required. */
#define CMD_map_image      0x20 /* u16:imageid */
#define CMD_map_song       0x21 /* u16:songid */
#define CMD_map_oob        0x22 /* u8:long u8:lat ; Only one map per plane needs to specify. If more, they must agree. */
#define CMD_map_parent     0x23 /* u16:mapid ; For this whole plane, use (mapid) for the world map position, this plane doesn't get a jigsaw puzzle. Caves and such. One level redirect only! */
#define CMD_map_position   0x40 /* s8:long s8:lat u8:plane u8:reserved */
#define CMD_map_root       0x41 /* u16:position u16:fld */
#define CMD_map_door       0x60 /* u16:position u16:mapid u16:dstposition u16:arg */
#define CMD_map_sprite     0x61 /* u16:position u16:spriteid u32:arg */
#define CMD_map_rsprite    0x62 /* u16:reserved u16:spriteid u32:arg ; May spawn anywhere vacant. One per command. */

#define CMD_sprite_solid     0x01 /* --- */
#define CMD_sprite_grabbable 0x02 /* --- */
#define CMD_sprite_image     0x20 /* u16:imageid */
#define CMD_sprite_tile      0x21 /* u8:tileid u8:xform */
#define CMD_sprite_type      0x23 /* u16:sprtype */
#define CMD_sprite_layer     0x24 /* s16:layer(0=default) */
#define CMD_sprite_battle    0x25 /* u16:battletype ; NS_sprtype_monster. Spawn point overrides. */
#define CMD_sprite_physics   0x40 /* (b32:physics)impassables */
#define CMD_sprite_hitbox    0x41 /* s8:left s8:right s8:top s8:bottom ; pixels ie m/16 */

#define NS_tilesheet_physics   1
#define NS_tilesheet_family    0
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_weight    0
#define NS_tilesheet_jigctab   2 /* RGB332 */

#define NS_physics_vacant     0
#define NS_physics_solid      1
#define NS_physics_water      2
#define NS_physics_hole       3 /* Like water, but no fishing etc. */
#define NS_physics_cliff      4 /* Jump over from the top, no return. */
#define NS_physics_hookable   5 /* Solid but can hookshot. */
#define NS_physics_safe       6 /* Vacant but monsters will never tread. */

// mapoob: How to handle requests for maps that don't exist on this plane.
#define NS_mapoob_null      1 /* (default) Unknown positions in this plane are invalid. */
#define NS_mapoob_loop      2 /* Loop around this axis. */
#define NS_mapoob_repeat    3 /* Clamp to the edge. ie repeat the last map forever. */
#define NS_mapoob_farloop   4 /* Repeat the last map for the size of the plane, then loop. So looping around is never the shortest path. */

#define NS_dir_nw   0x80
#define NS_dir_n    0x40
#define NS_dir_ne   0x20
#define NS_dir_w    0x10
#define NS_dir_mid  0x00
#define NS_dir_e    0x08
#define NS_dir_sw   0x04
#define NS_dir_s    0x02
#define NS_dir_se   0x01

// Inventory items.
#define NS_itemid_broom           1
#define NS_itemid_divining_rod    2
#define NS_itemid_hookshot        3
#define NS_itemid_fishpole        4
#define NS_itemid_match           5
#define NS_itemid_bugspray        6
#define NS_itemid_candy           7
#define NS_itemid_wand            8
#define NS_itemid_magnifier       9
#define NS_itemid_stick          10
#define NS_itemid_gold           11 /* Doesn't participate in inventory. */
#define NS_itemid_blood          12 /* '' */
#define NS_itemid_greenfish      13 /* '' */
#define NS_itemid_bluefish       14 /* '' */
#define NS_itemid_redfish        15 /* '' */

// Sprite controllers.
#define NS_sprtype_dummy          0 /* (u32)0 */
#define NS_sprtype_hero           1 /* (u32)0 */
#define NS_sprtype_monster        2 /* (u16:battletype)0 (u16)0 */
#define NS_sprtype_polefairy      3 /* (u24)0 (u8)0_left_1_right */
#define NS_sprtype_jigpiece       4 /* (u32)0 */
#define NS_sprtype_treasure       5 /* (u8:itemid)0 (u8:quantity)0 (u16:fld)0 */
#define NS_sprtype_candy          6 /* (u32)0 */
#define NS_sprtype_firepot        7 /* (u32)0 */
#define NS_sprtype_stick          8 /* (u32)0 */
#define NS_sprtype_npc            9 /* (u16:activity)0 (u16)0 */
#define FOR_EACH_SPRTYPE \
  _(dummy) \
  _(hero) \
  _(monster) \
  _(polefairy) \
  _(jigpiece) \
  _(treasure) \
  _(candy) \
  _(firepot) \
  _(stick) \
  _(npc)
  
// Battle controllers.
#define NS_battletype_fishing             1
#define NS_battletype_boomerang           2
#define NS_battletype_chopping            3
#define NS_battletype_exterminating       4
#define FOR_EACH_BATTLETYPE \
  _(fishing) \
  _(boomerang) \
  _(chopping) \
  _(exterminating)
  
// Store fields.
#define NS_fld_zero                0 /* Read-only, always zero. */
#define NS_fld_one                 1 /* Read-only, always one. */
#define NS_fld_always              2 /* Special to sprite_treasure. Value not used. */
#define NS_fld_if_depleted         3 /* '' */
#define NS_fld_hp                  4 /* 4 bits */
#define NS_fld_hpmax               8 /* 4 bits */
#define NS_fld_gold               12 /* 10 bits, limit artificially to 999. */
#define NS_fld_greenfish          22 /* 7 bits, limit artificially to 99. */
#define NS_fld_bluefish           29 /* 7 bits, limit artificially to 99. */
#define NS_fld_redfish            36 /* 7 bits, limit artificially to 99. */
#define NS_fld_gotstick           43 /* So we don't introduce Stick each time. It's the only item you lose repeatedly. */
#define NS_fld_cschest1           44
#define NS_fld_cschest2           45
#define NS_fld_root1              46
#define NS_fld_root2              47
#define NS_fld_root3              48
#define NS_fld_root4              49
#define NS_fld_root5              50
#define NS_fld_root6              51
#define NS_fld_root7              52
#define NS_fld_next               53

// NPC or similar triggerable activity.
#define NS_activity_carpenter 1
#define NS_activity_cheapside_clinic 2

/* Registry of map planes, since they aren't conveniently listed anywhere else.
 * 1 Outer world.
 * 2 Cheapside-Botire tunnel.
 */
  
#endif
