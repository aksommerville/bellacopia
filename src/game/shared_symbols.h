#ifndef SHARED_SYMBOLS_H
#define SHARED_SYMBOLS_H

#define EGGDEV_importUtil "stdlib,graf,font,res,text"

/* A map is exactly the width, and slightly taller, than the framebuffer.
 * It is important that maps not be smaller than the framebuffer: Camera will assume no more than 4 can be visible at a time.
 */
#define NS_sys_tilesize 16
#define NS_sys_mapw 20
#define NS_sys_maph 12
#define FBW 320
#define FBH 180

#define NS_dir_nw  0x80
#define NS_dir_n   0x40
#define NS_dir_ne  0x20
#define NS_dir_w   0x10
#define NS_dir_mid 0x00
#define NS_dir_e   0x08
#define NS_dir_sw  0x04
#define NS_dir_s   0x02
#define NS_dir_se  0x01

#define CMD_map_dark            0x01 /* --- */
#define CMD_map_image           0x20 /* u16:rid */
#define CMD_map_song            0x21 /* u16:rid */
#define CMD_map_wind            0x22 /* u8:edges u8:reserved ; edges:0x40,0x10,0x08,0x02 */
#define CMD_map_parent          0x23 /* u16:rid ; For jigsaw purposes, I belong to this other map. One level of redirection only, please. */
#define CMD_map_barrelhat       0x24 /* u16:pos ; Must also be switchable */
#define CMD_map_position        0x40 /* u8:lng u8:lat u8:z u8:reserved ; REQUIRED. (z==0) for singletons, and (lng,lat) must still be unique for them. */
#define CMD_map_switchable      0x41 /* u16:pos u16:fld ; tileid+1 if fld set */
#define CMD_map_treadle         0x42 /* u16:pos u16:fld ; tileid+1 if fld set, clears fld on load and sets when touched */
#define CMD_map_stompbox        0x43 /* u16:pos u16:fld ; tileid+1 if fld set, toggles when touched */
#define CMD_map_root            0x44 /* u16:pos u16:fld */
#define CMD_map_sprite          0x60 /* u16:pos u16:rid u32:arg */
#define CMD_map_rsprite         0x61 /* u16:rid u8:weight u8:limit u32:arg */
#define CMD_map_door            0x62 /* u16:pos u16:rid u16:dstpos u16:reserved */
#define CMD_map_compass         0x63 /* u16:pos u16:compass u16:fld u16:reserved ; For compass targets that can't be inferred generically. */
#define CMD_map_debugmsg        0xe0 /* ...:text ; Drawn hackfully over the map's image. For use during dev. */

#define CMD_sprite_image        0x20 /* u16:rid */
#define CMD_sprite_tile         0x21 /* u8:tileid u8:xform */
#define CMD_sprite_type         0x22 /* u16:sprtype */
#define CMD_sprite_layer        0x23 /* u16:layer ; hero at 100 */
#define CMD_sprite_physics      0x40 /* b32:physics ; which are impassable */
#define CMD_sprite_hitbox       0x41 /* s8:l s8:r s8:t s8:b ; pixels, default (-8,8,-8,8) */
#define CMD_sprite_groups       0x42 /* b32:sprgrp */
#define CMD_sprite_monster      0x60 /* u16:battle u4.4:radius u4.4:speed u16:name(RID_strings_battle) u16:reserved ; NS_sprtype_monster */

#define NS_tilesheet_physics 1
#define NS_tilesheet_jigctab 2 /* rgb332 */
#define NS_tilesheet_family 0
#define NS_tilesheet_neighbors 0
#define NS_tilesheet_weight 0

#define NS_physics_vacant 0
#define NS_physics_solid 1
#define NS_physics_water 2
#define NS_physics_grabbable 3 /* solid, but hookshot can grab */
#define NS_physics_safe 4 /* vacant, but monsters won't go unless chasing */
#define NS_physics_hole 5 /* water, but no splash */

/* Map planes. I doubt we'll actually use these symbols, they're just for manual documentation.
 */
#define NS_plane_singletons 0
#define NS_plane_outerworld 1
#define NS_plane_tunnel1 2 /* cheapside<~>botire */
#define NS_plane_caves1 3 /* mountains, where the goblins live. Expect multiple levels. */
#define NS_plane_labyrinth1 4 /* jungle */

#define NS_players_cpu_cpu 0 /* Princess vs monster. */
#define NS_players_cpu_man 1 /* I doubt we'll use this. */
#define NS_players_man_cpu 2 /* Dot vs monster, the usual case. */
#define NS_players_man_man 3 /* Two-player mode. */

#define NS_sprgrp_keepalive    0 /* All sprites are in this group. */
#define NS_sprgrp_deathrow     1 /* Everything here gets killed at the end of each update. */
#define NS_sprgrp_visible      2 /* Render order. Unusual for a sprite not to be here. */
#define NS_sprgrp_update       3 /* Get an update each frame. */
#define NS_sprgrp_solid        4 /* Participates in physics. */
#define NS_sprgrp_hazard       5 /* Damages the hero on contact. */
#define NS_sprgrp_hero         6 /* Should have exactly one member. */
#define NS_sprgrp_grabbable    7 /* Interacts with hookshot. */
#define NS_sprgrp_floating     8 /* Presumably solid, but doesn't interact with treadles etc. */
#define NS_sprgrp_light        9 /* Light source in darkened rooms. */
#define NS_sprgrp_monsterlike 10 /* Hero, princess, candy, and anything else monsters pay attention to. */

// Compass target classes. Value is strix in strings:item for the target's display name.
#define NS_compass_home 40
#define NS_compass_north 42
#define NS_compass_rootdevil 38
#define NS_compass_sidequest 39
#define NS_compass_heartcontainer 36
#define NS_compass_gold 27
#define NS_compass_auto 41

/* Everything you can pick up has an itemid.
 * So this includes gold, keys, powerups (things that don't go in inventory).
 */
#define NS_itemid_stick 1
#define NS_itemid_broom 2
#define NS_itemid_divining 3
#define NS_itemid_match 4
#define NS_itemid_wand 5
#define NS_itemid_fishpole 6
#define NS_itemid_bugspray 7
#define NS_itemid_potion 8
#define NS_itemid_hookshot 9
#define NS_itemid_candy 10
#define NS_itemid_magnifier 11
#define NS_itemid_vanishing 12
#define NS_itemid_compass 13
#define NS_itemid_gold 14
#define NS_itemid_greenfish 15
#define NS_itemid_bluefish 16
#define NS_itemid_redfish 17
#define NS_itemid_heart 18
#define NS_itemid_jigpiece 19
#define NS_itemid_bell 20
#define NS_itemid_heartcontainer 21
#define NS_itemid_barrelhat 22
#define NS_itemid_telescope 23
#define FOR_EACH_itemid \
  _(stick) \
  _(broom) \
  _(divining) \
  _(match) \
  _(wand) \
  _(fishpole) \
  _(bugspray) \
  _(potion) \
  _(hookshot) \
  _(candy) \
  _(magnifier) \
  _(vanishing) \
  _(compass) \
  _(gold) \
  _(greenfish) \
  _(bluefish) \
  _(redfish) \
  _(heart) \
  _(jigpiece) \
  _(bell) \
  _(heartcontainer) \
  _(barrelhat) \
  _(telescope)

/* NPC activities are hard-coded. Select one from this list.
 */
#define NS_activity_dialogue 1 /* (u16)strix ; strings:dialogue. Simple box of constant text. */
#define NS_activity_carpenter 2
#define NS_activity_brewer 3
#define NS_activity_bloodbank 4 /* (u16)price_per_heart */
#define NS_activity_fishwife 5
#define NS_activity_tolltroll 6 /* (u16:activity)appearance */
#define NS_activity_wargate 7 /* (u16)0 ; The guards outside the north tent preventing you from leaving until the war is over. */
#define NS_activity_knitter 8

#define NS_sprtype_dummy        0 /* (u32)0 */
#define NS_sprtype_hero         1 /* (u32)0 */
#define NS_sprtype_monster      2 /* (u32)0 */
#define NS_sprtype_treasure     3 /* (u8:itemid)0 (u8:quantity)0 (u16:fld)0 */
#define NS_sprtype_stick        4 /* (u32)0 */
#define NS_sprtype_npc          5 /* (u16:activity)0 (u16:activity_arg)0 */
#define NS_sprtype_jigpiece     6 /* (u32)0 */
#define NS_sprtype_candy        7 /* (u32)0 */
#define NS_sprtype_firepot      8 /* (u8:radius_m)3 (u24)0 */
#define NS_sprtype_rootdevil    9 /* (u16:fld)root0 (u16)0 */
#define NS_sprtype_bonfire     10 /* (u32)0 */
#define NS_sprtype_tolltroll   11 /* (u32)0 */
#define NS_sprtype_toast       12 /* (u32)0 */
#define FOR_EACH_sprtype \
  _(dummy) \
  _(hero) \
  _(monster) \
  _(treasure) \
  _(stick) \
  _(npc) \
  _(jigpiece) \
  _(candy) \
  _(firepot) \
  _(rootdevil) \
  _(bonfire) \
  _(tolltroll) \
  _(toast)
  
#define NS_battle_fishing 1
#define NS_battle_chopping 2
#define NS_battle_exterminating 3
#define NS_battle_boomerang 4
#define NS_battle_strangling 5
#define NS_battle_greenfish 6
#define NS_battle_bluefish 7
#define NS_battle_redfish 8
#define NS_battle_placeholder 9 /* Do not use in production! */
#define FOR_EACH_battle \
  _(fishing) \
  _(chopping) \
  _(exterminating) \
  _(boomerang) \
  _(strangling) \
  _(greenfish) \
  _(bluefish) \
  _(redfish) \
  _(placeholder)

/* "fld" are single bits.
 */
#define NS_fld_zero 0 /* readonly ; generally treated as a default, not necessarily literal zero */
#define NS_fld_one 1 /* readonly */
#define NS_fld_alsozero 2 /* readonly ; value is always zero, but key is nonzero in case that matters */
#define NS_fld_root1 3
#define NS_fld_root2 4
#define NS_fld_root3 5
#define NS_fld_root4 6
#define NS_fld_root5 7
#define NS_fld_root6 8
#define NS_fld_root7 9
#define NS_fld_hc1 10 /* heart container, in the cave of cheating (TEMP) */
#define NS_fld_toll_stick_requested 11
#define NS_fld_toll_compass_requested 12
#define NS_fld_toll_candy_requested 13
#define NS_fld_toll_paid 14
#define NS_fld_mayor 15
#define NS_fld_war_over 16
#define NS_fld_had_stick 17 /* For suppressing "got a stick" */
#define NS_fld_barrelhat1 18
#define NS_fld_barrelhat2 19
#define NS_fld_barrelhat3 20
#define NS_fld_barrelhat4 21
#define NS_fld_barrelhat5 22
#define NS_fld_barrelhat6 23
#define NS_fld_barrelhat7 24
#define NS_fld_barrelhat8 25
#define NS_fld_barrelhat9 26

/* "fld16" are 16 unsigned bits each.
 */
#define NS_fld16_zero 0 /* readonly */
#define NS_fld16_hp 1
#define NS_fld16_hpmax 2
#define NS_fld16_gold 3
#define NS_fld16_goldmax 4
#define NS_fld16_greenfish 5
#define NS_fld16_bluefish 6
#define NS_fld16_redfish 7
#define NS_fld16_compassoption 8

/* "clock" are floating-point seconds, and persist as integer ms.
 */
#define NS_clock_playtime 0
#define NS_clock_pausetime 1
#define NS_clock_battletime 2
  
#endif
