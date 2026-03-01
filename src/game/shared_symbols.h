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
#define CMD_map_cameralock      0x02 /* --- */
#define CMD_map_image           0x20 /* u16:rid */
#define CMD_map_song            0x21 /* u16:rid */
#define CMD_map_wind            0x22 /* u8:edges u8:reserved ; edges:0x40,0x10,0x08,0x02 */
#define CMD_map_parent          0x23 /* u16:rid ; For jigsaw purposes, I belong to this other map. One level of redirection only, please. */
#define CMD_map_barrelhat       0x24 /* u16:pos ; Must also be switchable */
#define CMD_map_pylon           0x25 /* u16:pos ; Used only by labyrinth, marks the top-left pylon so we can infer the rest. */
#define CMD_map_position        0x40 /* u8:lng u8:lat u8:z u8:reserved ; REQUIRED. (z==0) for singletons, and (lng,lat) must still be unique for them. */
#define CMD_map_switchable      0x41 /* u16:pos u16:fld ; tileid+1 if fld set */
#define CMD_map_treadle         0x42 /* u16:pos u16:fld ; tileid+1 if fld set, clears fld on load and sets when touched */
#define CMD_map_stompbox        0x43 /* u16:pos u16:fld ; tileid+1 if fld set, toggles when touched */
#define CMD_map_root            0x44 /* u16:pos u16:fld */
#define CMD_map_seal            0x46 /* u16:pos u16:id ; For crypto puzzle, but can be a generic "standing on something" poi. (1,2,3)=(bone,leaf,star) */
#define CMD_map_switchable2     0x47 /* u16:pos u16:fld ; Same as `switchable` but tileid+2 if set, for double-wide features. */
#define CMD_map_target          0x48 /* u16:pos u16:strix ; strings:item, loose unflagged targets for compass and princess. */
#define CMD_map_sprite          0x60 /* u16:pos u16:rid u32:arg */
#define CMD_map_rsprite         0x61 /* u16:rid u8:weight u8:limit u32:arg */
#define CMD_map_door            0x62 /* u16:pos u16:rid u16:dstpos u16:activity */
#define CMD_map_compass         0x63 /* u16:pos u16:compass u16:fld u16:reserved ; For compass targets that can't be inferred generically. */
#define CMD_map_buriedtreasure  0x64 /* u16:pos u16:fld u16:itemid u8:quantity u8:reserved */
#define CMD_map_bump            0x65 /* u16:pos u16:activity u16:arg_or_stringsid u16:strix ; Trigger static text or activity when colliding with a solid. For signs and such. */
#define CMD_map_triggeronce     0x66 /* u16:pos u16:fld u16:activity u16:arg ; Like a treadle, but it will only fire once ever. */
#define CMD_map_debugmsg        0xe0 /* ...:text ; Drawn hackfully over the map's image. For use during dev. */

#define CMD_sprite_noxform      0x01 /* --- ; npc */
#define CMD_sprite_image        0x20 /* u16:rid */
#define CMD_sprite_tile         0x21 /* u8:tileid u8:xform */
#define CMD_sprite_type         0x22 /* u16:sprtype */
#define CMD_sprite_layer        0x23 /* u16:layer ; hero at 100 */
#define CMD_sprite_physics      0x40 /* b32:physics ; which are impassable */
#define CMD_sprite_hitbox       0x41 /* s8:l s8:r s8:t s8:b ; pixels, default (-8,8,-8,8) */
#define CMD_sprite_groups       0x42 /* b32:sprgrp */
#define CMD_sprite_monster      0x60 /* u16:battle u4.4:radius u4.4:speed u16:name(RID_strings_battle) u16:reserved ; NS_sprtype_monster */
#define CMD_sprite_guild        0x61 /* u16:battle u16:name(RID_strings_battle) u16:fld u16:reserved ; NS_sprtype_guild */

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
#define NS_physics_vanishable 6 /* solid, unless you're using vanishing cream */

/* Map planes. I doubt we'll actually use these symbols, they're just for manual documentation.
 */
#define NS_plane_singletons 0
#define NS_plane_outerworld 1
#define NS_plane_tunnel1 2 /* cheapside<~>botire */
#define NS_plane_caves1 3 /* mountains, where the goblins live. Expect multiple levels. */
#define NS_plane_labyrinth1 4 /* jungle */
#define NS_plane_temple_ground 5

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
#define NS_sprgrp_moveable    11 /* Can be moved by external forces like bomb and snowglobe. */

#define NS_transition_cut 0
#define NS_transition_spotlight 1
#define NS_transition_crossfade 2
#define NS_transition_fadeblack 3

// Compass target classes. Value is strix in strings:item for the target's display name.
#define NS_compass_home 40
#define NS_compass_north 42
#define NS_compass_rootdevil 38
#define NS_compass_sidequest 39
#define NS_compass_heartcontainer 36
#define NS_compass_gold 27
#define NS_compass_auto 41
#define NS_compass_castle 66

// Face choices for battle.
#define NS_face_monster 0 /* "The monster", whatever the game prefers. */
#define NS_face_dot 1
#define NS_face_princess 2

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
#define NS_itemid_shovel 24
#define NS_itemid_pepper 25
#define NS_itemid_text 26 /* Not a real item. For modal_battle_add_consequence, with a strix in strings:battle. */
#define NS_itemid_letter1 27 /* blue to red */
#define NS_itemid_letter2 28 /* ...improved */
#define NS_itemid_letter3 29 /* red to blue */
#define NS_itemid_letter4 30 /* ...improved */
#define NS_itemid_bomb 31
#define NS_itemid_stopwatch 32
#define NS_itemid_busstop 33
#define NS_itemid_snowglobe 34
#define NS_itemid_tapemeasure 35
#define NS_itemid_phonograph 36
#define NS_itemid_crystal 37
#define NS_itemid_glove 38
#define NS_itemid_marionette 39
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
  _(telescope) \
  _(shovel) \
  _(pepper) \
  _(text) \
  _(letter1) \
  _(letter2) \
  _(letter3) \
  _(letter4) \
  _(bomb) \
  _(stopwatch) \
  _(busstop) \
  _(snowglobe) \
  _(tapemeasure) \
  _(phonograph) \
  _(crystal) \
  _(glove) \
  _(marionette)

/* NPC activities are hard-coded. Select one from this list.
 */
#define NS_activity_dialogue 1 /* (u16)strix ; strings:dialogue. Simple box of constant text. */
#define NS_activity_carpenter 2
#define NS_activity_brewer 3
#define NS_activity_bloodbank 4 /* (u16)price_per_heart */
#define NS_activity_fishwife 5
#define NS_activity_tolltroll 6 /* (u16:itemid)appearance ; The three-part main quest. */
#define NS_activity_wargate 7 /* (u16)0 ; The guards outside the north tent preventing you from leaving until the war is over. */
#define NS_activity_knitter 8
#define NS_activity_magneticnorth 9
#define NS_activity_thingwalla 10
#define NS_activity_king 11
#define NS_activity_fishprocessor 12
#define NS_activity_jaildoor 13
#define NS_activity_kidnap 14
#define NS_activity_escape 15
#define NS_activity_cryptmsg 16 /* (u16)which: 1,2,3,4,5,6,7: Bone Room, Leaf Room, 5x Skull Lake */
#define NS_activity_linguist 17
#define NS_activity_logproblem1 18
#define NS_activity_logproblem2 19
#define NS_activity_board_of_elections 20
#define NS_activity_cheat_store 21 /* (u16)which: 1,2,3 */
#define NS_activity_cheat_giveaway 22
#define NS_activity_capnred 23
#define NS_activity_capnblue 24
#define NS_activity_poet 25
#define NS_activity_enter_labyrinth 26
#define NS_activity_tree 27 /* (u16)fld */
#define NS_activity_override_outerworld_song 28
#define NS_activity_unoverride_outerworld_song 29
#define NS_activity_castleshop 30
#define NS_activity_templeshop 31
#define NS_activity_generic_tolltroll 32 /* (price<<12)|fld */
#define NS_activity_phonograph 33

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
#define NS_sprtype_tolltroll   11 /* (u16:cost)0 (u16:fld)0 ; Zeroes for the three-part fetch quest */
#define NS_sprtype_toast       12 /* (u32)0 */
#define NS_sprtype_princess    13 /* (u32)0 */
#define NS_sprtype_setfld      14 /* (u16:fld) (u16)0 */
#define NS_sprtype_ornament2x2 15 /* (u32)0 */
#define NS_sprtype_tvnews      16 /* (u32)0 */
#define NS_sprtype_guild       17 /* (u32)0 */
#define NS_sprtype_escalator   18 /* (u8)h (u24)0 */
#define NS_sprtype_statue      19 /* (u32)0 */
#define NS_sprtype_statuemaze  20 /* (u32)0 */
#define NS_sprtype_bomb        21 /* (u32)0 */
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
  _(toast) \
  _(princess) \
  _(setfld) \
  _(ornament2x2) \
  _(tvnews) \
  _(guild) \
  _(escalator) \
  _(statue) \
  _(statuemaze) \
  _(bomb)
  
#define NS_battle_fishing 1
#define NS_battle_chopping 2
#define NS_battle_exterminating 3
#define NS_battle_boomerang 4
#define NS_battle_strangling 5
#define NS_battle_greenfish 6
#define NS_battle_bluefish 7
#define NS_battle_redfish 8
#define NS_battle_placeholder 9 /* Do not use in production! */
#define NS_battle_throwing 10
#define NS_battle_stealing 11
#define NS_battle_regex 12
#define NS_battle_racketeering 13
#define NS_battle_laziness 14
#define NS_battle_hiring 15
#define NS_battle_gobbling 16
#define NS_battle_erudition 17
#define NS_battle_crying 18
#define NS_battle_cobbling 19
#define NS_battle_apples 20
#define NS_battle_election 21
#define NS_battle_watching 22
#define NS_battle_flapjack 23
#define NS_battle_topping 24
#define NS_battle_stacking 25
#define NS_battle_shaking 26
#define NS_battle_cakecarrying 27
#define NS_battle_traffic 28
#define NS_battle_cheesecutting 29
#define NS_battle_latin 30
#define NS_battle_rescuing 31
#define NS_battle_sumohorse 32
#define NS_battle_fencing 33
#define NS_battle_jeter 34
#define NS_battle_homerunderby 35
#define NS_battle_dissection 36
#define NS_battle_cpr 37
#define NS_battle_stenography 38
#define NS_battle_sorting 39
#define NS_battle_slapping 40
#define NS_battle_shuffling 41
#define NS_battle_cheating 42
#define NS_battle_plumbing 43
#define NS_battle_building 44
#define NS_battle_sawing 45
#define NS_battle_lawnmowing 46
#define NS_battle_telekinesis 47
#define NS_battle_smashing 48
#define NS_battle_pushing 49
#define NS_battle_petrifying 50
#define NS_battle_mindcontrol 51
#define NS_battle_golf 52
#define NS_battle_counting 53
#define NS_battle_chess 54
#define NS_battle_broomrace 55
#define NS_battle_armwrestling 56
#define FOR_EACH_battle \
  _(fishing) \
  _(chopping) \
  _(exterminating) \
  _(boomerang) \
  _(strangling) \
  _(greenfish) \
  _(bluefish) \
  _(redfish) \
  _(placeholder) \
  _(throwing) \
  _(stealing) \
  _(regex) \
  _(racketeering) \
  _(laziness) \
  _(hiring) \
  _(gobbling) \
  _(erudition) \
  _(crying) \
  _(cobbling) \
  _(apples) \
  _(election) \
  _(watching) \
  _(flapjack) \
  _(topping) \
  _(stacking) \
  _(shaking) \
  _(cakecarrying) \
  _(traffic) \
  _(cheesecutting) \
  _(latin) \
  _(rescuing) \
  _(sumohorse) \
  _(fencing) \
  _(jeter) \
  _(homerunderby) \
  _(dissection) \
  _(cpr) \
  _(stenography) \
  _(sorting) \
  _(slapping) \
  _(shuffling) \
  _(cheating) \
  _(plumbing) \
  _(building) \
  _(sawing) \
  _(lawnmowing) \
  _(telekinesis) \
  _(smashing) \
  _(pushing) \
  _(petrifying) \
  _(mindcontrol) \
  _(golf) \
  _(counting) \
  _(chess) \
  _(broomrace) \
  _(armwrestling)

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
#define NS_fld_hc1 10 /* heart container, at castleshop */
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
#define NS_fld_target_hc 27
#define NS_fld_target_rootdevil 28
#define NS_fld_target_sidequest 29
#define NS_fld_target_gold 30
#define NS_fld_rescued_princess 31
#define NS_fld_bt1 32
#define NS_fld_bt2 33
#define NS_fld_bt3 34
#define NS_fld_bt4 35
#define NS_fld_jailopen 36
#define NS_fld_jailkey 37
#define NS_fld_kidnapped 38
#define NS_fld_escaped 39
#define NS_fld_bonedoor 40
#define NS_fld_leafdoor 41
#define NS_fld_stardoor 42
#define NS_fld_no_encryption 43 /* Not for production. */
#define NS_fld_bought_alphabet 44
#define NS_fld_bought_translation 45
#define NS_fld_endorse_food 46
#define NS_fld_endorse_public 47
#define NS_fld_endorse_athlete 48
#define NS_fld_endorse_hospital 49
#define NS_fld_endorse_casino 50
#define NS_fld_endorse_labor 51
#define NS_fld_election_start 52 /* Set when the election quest begins, and stays on forever. At the end, `mayor` is also set. */
#define NS_fld_capnred_happy 53 /* Midway thru the war. */
#define NS_fld_tree1 54
#define NS_fld_tree2 55
#define NS_fld_tree3 56
#define NS_fld_tree4 57
#define NS_fld_tree5 58
#define NS_fld_tree6 59
#define NS_fld_tree7 60
#define NS_fld_tree8 61
#define NS_fld_tree9 62
#define NS_fld_tree10 63
#define NS_fld_tree11 64
#define NS_fld_tree12 65
#define NS_fld_tree13 66
#define NS_fld_tree14 67
#define NS_fld_tree15 68
#define NS_fld_tree16 69
#define NS_fld_story1 70
#define NS_fld_story2 71
#define NS_fld_story3 72
#define NS_fld_story4 73
#define NS_fld_story5 74
#define NS_fld_story6 75
#define NS_fld_story7 76
#define NS_fld_story8 77
#define NS_fld_story9 78
#define NS_fld_story10 79
#define NS_fld_story11 80
#define NS_fld_story12 81
#define NS_fld_story13 82
#define NS_fld_story14 83
#define NS_fld_story15 84
#define NS_fld_story16 85
#define NS_fld_forest_toll 86

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
#define NS_fld16_cryptmsg_seed 9
#define NS_fld16_labyrinth_seed 10
#define NS_fld16_phonograph 11 /* Zero for default, or explicit song rid for outerworld. */

/* "clock" are floating-point seconds, and persist as integer ms.
 */
#define NS_clock_playtime 0
#define NS_clock_pausetime 1
#define NS_clock_battletime 2
  
#endif
