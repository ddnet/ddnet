#include <game/localization.h>

#include "editor.h"

// DDNet entity explanations by Lady Saavik
// TODO: Add other entities' tiles' explanations and improve new ones

const char *CEditor::ExplainDDNet(int Tile, int Layer)
{
	switch(Tile)
	{
	case TILE_AIR:
		return Localize("EMPTY: Can be used as an eraser.", "Editor Explanation");
	case TILE_SOLID:
		if(Layer == LAYER_GAME)
			return Localize("HOOKABLE: It's possible to hook and collide with it.", "Editor Explanation");
		break;
	case TILE_DEATH:
		if(Layer == LAYER_GAME)
			return Localize("KILL: Kills the tee.", "Editor Explanation");
		break;
	case TILE_NOHOOK:
		if(Layer == LAYER_GAME)
			return Localize("UNHOOKABLE: It's not possible to hook it, but can collide with it.", "Editor Explanation");
		break;
	case TILE_NOLASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("LASER BLOCKER: Doesn't let DRAGGING & SPINNING LASER and PLASMA TURRET reach tees through it.", "Editor Explanation");
		break;
	case TILE_THROUGH_CUT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOKTHROUGH: Shortcut for new hookthrough.", "Editor Explanation");
		break;
	case TILE_THROUGH_ALL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOKTHROUGH: Combined with a collision tile is new hookthrough, otherwise stops hooks, from all directions.", "Editor Explanation");
		break;
	case TILE_THROUGH_DIR:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOKTHROUGH: Combined with a collision tile is new hookthrough, otherwise stops hook, from one direction.", "Editor Explanation");
		break;
	case TILE_THROUGH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOKTHROUGH: Combined with (UN)HOOKABLE tiles, allows to hook through the walls.", "Editor Explanation");
		break;
	case TILE_JUMP:
		if(Layer == LAYER_SWITCH)
			return Localize("JUMP: Sets defined amount of jumps (default is 2).", "Editor Explanation");
		break;
	case TILE_FREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("FREEZE: Freezes tees for 3 seconds.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("FREEZE: Freezes tees for defined amount of seconds.", "Editor Explanation");
		break;
	case TILE_UNFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("UNFREEZE: Unfreezes tees immediately.", "Editor Explanation");
		break;
	case TILE_TELEINEVIL:
		if(Layer == LAYER_TELE)
			return Localize("RED TELEPORT: After falling into this tile, tees appear on TO with the same number. Speed and hooks are reset.", "Editor Explanation");
		break;
	case TILE_DFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DEEP FREEZE: Permanent freeze. Only UNDEEP tile can cancel this effect.", "Editor Explanation");
		break;
	case TILE_DUNFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("UNDEEP: Removes DEEP FREEZE effect.", "Editor Explanation");
		break;
	case TILE_LFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LIVE FREEZE: Live frozen tees cannot move or jump, while hook and weapons can still be used.", "Editor Explanation");
		break;
	case TILE_LUNFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LIVE UNFREEZE: Removes LIVE FREEZE effect.", "Editor Explanation");
		break;
	case TILE_TELEINWEAPON:
		if(Layer == LAYER_TELE)
			return Localize("WEAPON TELEPORT: Teleports bullets shot into it to TELEPORT TO, where it comes out. Direction, angle and length are kept.", "Editor Explanation");
		break;
	case TILE_TELEINHOOK:
		if(Layer == LAYER_TELE)
			return Localize("HOOK TELEPORT: Teleports hooks entering into it to TELEPORT TO, where it comes out. Direction, angle and length are kept.", "Editor Explanation");
		break;
	case TILE_WALLJUMP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("WALLJUMP: Placed next to a wall. Enables climbing up the wall.", "Editor Explanation");
		break;
	case TILE_EHOOK_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("ENDLESS HOOK: Activates endless hook.", "Editor Explanation");
		break;
	case TILE_EHOOK_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("ENDLESS HOOK OFF: Deactivates endless hook.", "Editor Explanation");
		break;
	case TILE_HIT_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HIT OTHERS: You can hit others.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("HIT OTHERS: You can activate hitting others for single weapons, using delay number to select which.", "Editor Explanation");
		break;
	case TILE_HIT_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HIT OTHERS: You can't hit others.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("HIT OTHERS: You can deactivate hitting others for single weapons, using delay number to select which.", "Editor Explanation");
		break;
	case TILE_SOLO_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SOLO: You are now in a solo part.", "Editor Explanation");
		break;
	case TILE_SOLO_DISABLE: // also TILE_SWITCHTIMEDOPEN
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SOLO: You are now out of the solo part.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("TIME SWITCH: Activates switch (e.g. closes door) with the same number for a set amount of seconds.", "Editor Explanation");
		break;
	case TILE_SWITCHTIMEDCLOSE:
		if(Layer == LAYER_SWITCH)
			return Localize("TIME SWITCH: Deactivates switch (e.g. opens door) with the same number for a set amount of seconds.", "Editor Explanation");
		break;
	case TILE_SWITCHOPEN:
		if(Layer == LAYER_SWITCH)
			return Localize("SWITCH: Activates switch (e.g. closes door) with the same number.", "Editor Explanation");
		break;
	case TILE_SWITCHCLOSE:
		if(Layer == LAYER_SWITCH)
			return Localize("SWITCH: Deactivates switch (e.g. opens door) with the same number.", "Editor Explanation");
		break;
	case TILE_TELEIN:
		if(Layer == LAYER_TELE)
			return Localize("BLUE TELEPORT: After falling into this tile, tees appear on TO with the same number. Speed and hook are kept.", "Editor Explanation");
		break;
	case TILE_TELEOUT:
		if(Layer == LAYER_TELE)
			return Localize("TELEPORT TO: Destination tile for FROMs, WEAPON & HOOK TELEPORTs with the same numbers.", "Editor Explanation");
		break;
	case TILE_BOOST:
		if(Layer == LAYER_SPEEDUP)
			return Localize("SPEEDUP: Gives tee defined speed. Arrow shows direction and angle.", "Editor Explanation");
		break;
	case TILE_TELECHECK:
		if(Layer == LAYER_TELE)
			return Localize("CHECKPOINT TELEPORT: After having touched this tile, any CFRM will teleport you to CTO with the same number.", "Editor Explanation");
		break;
	case TILE_TELECHECKOUT:
		if(Layer == LAYER_TELE)
			return Localize("CHECKPOINT TELEPORT TO: Tees will appear here after touching TELEPORT CHECKPOINT with the same number and falling into CFROM TELEPORT.", "Editor Explanation");
		break;
	case TILE_TELECHECKIN:
		if(Layer == LAYER_TELE)
			return Localize("BLUE CHECKPOINT TELEPORT: Sends tees to CTO with the same number as the last touched TELEPORT CHECKPOINT. Speed and hook are kept.", "Editor Explanation");
		break;
	case TILE_REFILL_JUMPS:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("REFILL JUMPS: Restores all jumps.", "Editor Explanation");
		break;
	case TILE_START:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("START: Starts counting your race time.", "Editor Explanation");
		break;
	case TILE_FINISH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("FINISH: End of race.", "Editor Explanation");
		break;
	case TILE_STOP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("STOPPER: You can hook and shoot through it. You can't go through it against the arrow.", "Editor Explanation");
		break;
	case TILE_STOPS:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("STOPPER: You can hook and shoot through it. You can't go through it against the arrows.", "Editor Explanation");
		break;
	case TILE_STOPA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("STOPPER: You can hook and shoot through it. You can't go through it.", "Editor Explanation");
		break;
	case TILE_TELECHECKINEVIL:
		if(Layer == LAYER_TELE)
			return Localize("RED CHECKPOINT TELEPORT: Send tees to CTO with the same number as the last touched TELEPORT CHECKPOINT. Speed and hook are reset.", "Editor Explanation");
		break;
	case TILE_CP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SPEEDER: Causes weapons, SHIELD, HEART and SPINNING LASER to move slowly.", "Editor Explanation");
		break;
	case TILE_CP_F:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SPEEDER: Causes weapons, SHIELD, HEART and SPINNING LASER to move quickly.", "Editor Explanation");
		break;
	case TILE_TUNE:
		if(Layer == LAYER_TUNE)
			return Localize("TUNE ZONE: Area where defined tunes work.", "Editor Explanation");
		break;
	case TILE_OLDLASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("GLOBAL OLD SHOTGUN: Shotgun drags others always towards the shooter, even after having bounced. Shooter can't hit themselves. Place only one tile somewhere on the map.", "Editor Explanation");
		break;
	case TILE_NPC:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("GLOBAL COLLISION OFF: Nobody can collide with others. Place only one tile somewhere on the map.", "Editor Explanation");
		break;
	case TILE_EHOOK:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("GLOBAL ENDLESS HOOK ON: Everyone has endless hook. Place only one tile somewhere on the map.", "Editor Explanation");
		break;
	case TILE_NOHIT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("GLOBAL HIT OTHERS OFF: Nobody can hit others. Place only one tile somewhere on the map.", "Editor Explanation");
		break;
	case TILE_NPH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("GLOBAL HOOK OTHERS OFF: Nobody can hook others. Place only one tile somewhere on the map.", "Editor Explanation");
		break;
	case TILE_UNLOCK_TEAM:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("UNLOCK TEAM: Forces team to be unlocked so that team doesn't get killed when one dies.", "Editor Explanation");
		break;
	case TILE_ADD_TIME:
		if(Layer == LAYER_SWITCH)
			return Localize("PENALTY: Adds time to your current race time. Opposite of BONUS.", "Editor Explanation");
		break;
	case TILE_NPC_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("COLLISION OFF: You can't collide with others.", "Editor Explanation");
		break;
	case TILE_UNLIMITED_JUMPS_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SUPER JUMP OFF: You don't have unlimited air jumps.", "Editor Explanation");
		break;
	case TILE_JETPACK_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("JETPACK OFF: You lose your jetpack gun.", "Editor Explanation");
		break;
	case TILE_NPH_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOK OTHERS OFF: You can't hook others.", "Editor Explanation");
		break;
	case TILE_SUBTRACT_TIME:
		if(Layer == LAYER_SWITCH)
			return Localize("BONUS: Subtracts time from your current race time. Opposite of PENALTY.", "Editor Explanation");
		break;
	case TILE_NPC_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("COLLISION: You can collide with others.", "Editor Explanation");
		break;
	case TILE_UNLIMITED_JUMPS_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SUPER JUMP: You have unlimited air jumps.", "Editor Explanation");
		break;
	case TILE_JETPACK_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("JETPACK: You have a jetpack gun.", "Editor Explanation");
		break;
	case TILE_NPH_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("HOOK OTHERS: You can hook others.", "Editor Explanation");
		break;
	case TILE_CREDITS_1:
	case TILE_CREDITS_2:
	case TILE_CREDITS_3:
	case TILE_CREDITS_4:
	case TILE_CREDITS_5:
	case TILE_CREDITS_6:
	case TILE_CREDITS_7:
	case TILE_CREDITS_8:
		return Localize("CREDITS: Who designed the entities.", "Editor Explanation");
	case TILE_ENTITIES_OFF_1:
	case TILE_ENTITIES_OFF_2:
		return Localize("ENTITIES OFF SIGN: Informs people playing with entities about important marks, tips, information or text on the map.", "Editor Explanation");
	// Entities
	case ENTITY_OFFSET + ENTITY_SPAWN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SPAWN: Here tees will appear after joining the game or dying somewhere on the map.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_SPAWN_RED:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SPAWN: Red team members spawn here, same as normal spawn in DDRace.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_SPAWN_BLUE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("SPAWN: Blue team members spawn here, same as normal spawn in DDRace.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_FLAGSTAND_RED:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("FLAG: Not used in DDRace. Place where red team flag is.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_FLAGSTAND_BLUE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("FLAG: Not used in DDRace. Place where blue team flag is.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_1:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SHIELD: Takes all weapons (except hammer and pistol) away.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_HEALTH_1:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("HEART: Works like a FREEZE tile. Freezes tees for 3 seconds by default.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_SHOTGUN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SHOTGUN: Drags the tees towards it. Bounces off the walls.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_GRENADE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("GRENADE LAUNCHER: Throws exploding bullets. Also known as rocket.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_POWERUP_NINJA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("NINJA: Makes you invisible in the darkest nights.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_LASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER: Unfreezes hit tee. Bounces off the walls. Also known as laser.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_FAST_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, fast.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_NORMAL_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, medium speed.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SLOW_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, slow.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_STOP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("NON-SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SLOW_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, slow.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_NORMAL_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, medium speed.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_FAST_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, fast.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SHORT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 3 tiles long.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_MEDIUM:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 6 tiles long.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_LONG:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 9 tiles long.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_SLOW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, slow.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, medium speed.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_FAST:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, fast.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_SLOW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, slow.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, medium speed.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_FAST:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, fast.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("PLASMA TURRET: Shoots plasma bullets at the closest tee. They explode on an obstactle they hit (wall or tee).", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAF:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("PLASMA TURRET: Shoots plasma bullets that work like FREEZE at the closest tee.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_PLASMA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("PLASMA TURRET: Shoots plasma bullets that work like FREEZE at the closest tee. They also explode on an obstactly they hit (wall or tee).", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAU:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("PLASMA TURRET: Shoots plasma bullets that work like UNFREEZE at the closest tee.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_CRAZY_SHOTGUN_EX:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("EXPLODING BULLET: Bounces off the walls with explosion. Touching the bullet works like FREEZE tile (freezes for 3 seconds by default).", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_CRAZY_SHOTGUN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("BULLET: Bounces off the walls without explosion. Touching the bullet works like FREEZE tile (freezes for 3 seconds by default).", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_SHOTGUN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("SHOTGUN SHIELD: Takes shotgun away.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_GRENADE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("GRENADE SHIELD: Takes grenade away.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_NINJA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("NINJA SHIELD: Takes ninja away.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_LASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("LASER SHIELD: Takes laser away.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_WEAK:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Weak.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Medium strength.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_STRONG:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Strong.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_WEAK_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Weak.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_NORMAL_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Medium strength.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_STRONG_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Strong.", "Editor Explanation");
		break;
	case ENTITY_OFFSET + ENTITY_DOOR:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return Localize("DOOR: Combined with LASER LENGTH creates doors. Doesn't allow to go through it (only with NINJA).", "Editor Explanation");
		break;
	case TILE_TELE_GUN_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELEGUN: Turn gun on as telegun weapon.", "Editor Explanation");
		break;
	case TILE_TELE_GUN_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELEGUN OFF: Turn gun off as telegun weapon.", "Editor Explanation");
		break;
	case TILE_ALLOW_TELE_GUN:
		if(Layer == LAYER_FRONT)
			return Localize("TELEGUN: Place on top of a collision tile, activates a spot to teleport to, cancels movement.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("TELEGUN: Place on top of a collision tile, activates a spot to teleport to, cancels movement, for single weapons, using delay number to select which.", "Editor Explanation");
		break;
	case TILE_ALLOW_BLUE_TELE_GUN:
		if(Layer == LAYER_FRONT)
			return Localize("TELEGUN: Place on top of a collision tile, activates a spot to teleport to, preserves movement.", "Editor Explanation");
		if(Layer == LAYER_SWITCH)
			return Localize("TELEGUN: Place on top of a collision tile, activates a spot to teleport to, preserves movement, for single weapons, using delay number to select which.", "Editor Explanation");
		break;
	case TILE_TELE_GRENADE_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELEGRENADE: Turn grenade on as telegun weapon.", "Editor Explanation");
		break;
	case TILE_TELE_GRENADE_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELEGRENADE OFF: Turn grenade off as telegun weapon.", "Editor Explanation");
		break;
	case TILE_TELE_LASER_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELELASER: Turn laser on as telegun weapon.", "Editor Explanation");
		break;
	case TILE_TELE_LASER_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return Localize("TELELASER OFF: Turn laser off as telegun weapon.", "Editor Explanation");
		break;
	}
	if(Tile >= TILE_TIME_CHECKPOINT_FIRST && Tile <= TILE_TIME_CHECKPOINT_LAST && (Layer == LAYER_GAME || Layer == LAYER_FRONT))
		return Localize("TIME CHECKPOINT: Compares your current race time with your record to show you whether you are running faster or slower.", "Editor Explanation");
	return nullptr;
}

const char *CEditor::ExplainFNG(int Tile, int Layer)
{
	switch(Tile)
	{
	case TILE_PUB_AIR:
		return Localize("EMPTY: Can be used as an eraser.", "Editor Explanation");
	case TILE_PUB_HOOKABLE:
		if(Layer == LAYER_GAME)
			return Localize("HOOKABLE: It's possible to hook and collide with it.", "Editor Explanation");
		break;
	case TILE_PUB_DEATH:
		if(Layer == LAYER_GAME)
			return Localize("DEATH: Kills the tee.", "Editor Explanation");
		break;
	case TILE_PUB_UNHOOKABLE:
		if(Layer == LAYER_GAME)
			return Localize("UNHOOKABLE: It's not possible to hook it, but can collide with it.", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_GOLD:
		if(Layer == LAYER_GAME)
			return Localize("GOLDEN SPIKE: Kills the tee and gives points to the killer. (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_NORMAL:
		if(Layer == LAYER_GAME)
			return Localize("NORMAL SPIKE: Kills the tee and gives points to the killer. (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_RED:
		if(Layer == LAYER_GAME)
			return Localize("RED SPIKE: Red team spikes. Gives negative points when killer is in blue team (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("BLUE SPIKE: Blue team spikes. Gives negative points when killer is in red team (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SCORE_RED:
		if(Layer == LAYER_GAME)
			return Localize("SCORE: Old tile used for showing red team score using laser text. No longer usable in FNG2", "Editor Explanation");
		break;
	case TILE_FNG_SCORE_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("SCORE: Old tile used for showing blue team score using laser text. No longer usable in FNG2", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_GREEN:
		if(Layer == LAYER_GAME)
			return Localize("GREEN SPIKE: Kills the tee and gives points to the killer. (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_PURPLE:
		if(Layer == LAYER_GAME)
			return Localize("PURPLE SPIKE: Kills the tee and gives points to the killer. (Amount of points given is set inside the server)", "Editor Explanation");
		break;
	case TILE_FNG_SPAWN:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Here tees will appear after joining the game or dying.", "Editor Explanation");
		break;
	case TILE_FNG_SPAWN_RED:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Red team members spawn here.", "Editor Explanation");
		break;
	case TILE_FNG_SPAWN_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Blue team members spawn here.", "Editor Explanation");
		break;
	case TILE_FNG_FLAG_RED:
		if(Layer == LAYER_GAME)
			return Localize("FLAG: Not used in FNG. Place where red team flag is.", "Editor Explanation");
		break;
	case TILE_FNG_FLAG_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("FLAG: Not used in FNG. Place where blue team flag is.", "Editor Explanation");
		break;
	case TILE_FNG_SHIELD:
		if(Layer == LAYER_GAME)
			return Localize("SHIELD: Does nothing in FNG.", "Editor Explanation");
		break;
	case TILE_FNG_HEART:
		if(Layer == LAYER_GAME)
			return Localize("HEART: Does nothing in FNG.", "Editor Explanation");
		break;
	case TILE_FNG_SHOTGUN:
		if(Layer == LAYER_GAME)
			return Localize("SHOTGUN: Not used in FNG. Gives you shotgun with 10 charges.", "Editor Explanation");
		break;
	case TILE_FNG_GRENADE:
		if(Layer == LAYER_GAME)
			return Localize("GRENADE: Gives you grenade weapon with 10 charges. Not really useful in FNG.", "Editor Explanation");
		break;
	case TILE_FNG_NINJA:
		if(Layer == LAYER_GAME)
			return Localize("NINJA: Does nothing in FNG.", "Editor Explanation");
		break;
	case TILE_FNG_LASER:
		if(Layer == LAYER_GAME)
			return Localize("LASER: Gives you laser weapon with 10 charges. Not really useful in FNG.", "Editor Explanation");
		break;
	case TILE_FNG_SPIKE_OLD1:
	case TILE_FNG_SPIKE_OLD2:
	case TILE_FNG_SPIKE_OLD3:
		if(Layer == LAYER_GAME)
			return Localize("SPIKE: Old FNG spikes. Deprecated.", "Editor Explanation");
		break;
	}
	if((Tile >= TILE_PUB_CREDITS1 && Tile <= TILE_PUB_CREDITS8) && Layer == LAYER_GAME)
		return Localize("CREDITS: Who designed the entities.", "Editor Explanation");
	else if((Tile == TILE_PUB_ENTITIES_OFF1 || Tile == TILE_PUB_ENTITIES_OFF2) && Layer == LAYER_GAME)
		return Localize("ENTITIES OFF SIGN: Informs people playing with entities about important marks, tips, information or text on the map.", "Editor Explanation");
	return nullptr;
}

const char *CEditor::ExplainVanilla(int Tile, int Layer)
{
	switch(Tile)
	{
	case TILE_PUB_AIR:
		return Localize("EMPTY: Can be used as an eraser.", "Editor Explanation");
	case TILE_PUB_HOOKABLE:
		if(Layer == LAYER_GAME)
			return Localize("HOOKABLE: It's possible to hook and collide with it.", "Editor Explanation");
		break;
	case TILE_PUB_DEATH:
		if(Layer == LAYER_GAME)
			return Localize("DEATH: Kills the tee.", "Editor Explanation");
		break;
	case TILE_PUB_UNHOOKABLE:
		if(Layer == LAYER_GAME)
			return Localize("UNHOOKABLE: It's not possible to hook it, but can collide with it.", "Editor Explanation");
		break;
	case TILE_VANILLA_SPAWN:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Here tees will appear after joining the game or dying.", "Editor Explanation");
		break;
	case TILE_VANILLA_SPAWN_RED:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Red team members spawn here.", "Editor Explanation");
		break;
	case TILE_VANILLA_SPAWN_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("SPAWN: Blue team members spawn here.", "Editor Explanation");
		break;
	case TILE_VANILLA_FLAG_RED:
		if(Layer == LAYER_GAME)
			return Localize("FLAG: Place where red team flag is.", "Editor Explanation");
		break;
	case TILE_VANILLA_FLAG_BLUE:
		if(Layer == LAYER_GAME)
			return Localize("FLAG: Place where blue team flag is.", "Editor Explanation");
		break;
	case TILE_VANILLA_SHIELD:
		if(Layer == LAYER_GAME)
			return Localize("SHIELD: Gives player +1 shield.", "Editor Explanation");
		break;
	case TILE_VANILLA_HEART:
		if(Layer == LAYER_GAME)
			return Localize("HEART: Gives player +1 health", "Editor Explanation");
		break;
	case TILE_VANILLA_SHOTGUN:
		if(Layer == LAYER_GAME)
			return Localize("SHOTGUN: Gives you shotgun weapon with 10 charges.", "Editor Explanation");
		break;
	case TILE_VANILLA_GRENADE:
		if(Layer == LAYER_GAME)
			return Localize("GRENADE: Gives you grenade weapon with 10 charges.", "Editor Explanation");
		break;
	case TILE_VANILLA_NINJA:
		if(Layer == LAYER_GAME)
			return Localize("NINJA: Gives you ninja for a period of time.", "Editor Explanation");
		break;
	case TILE_VANILLA_LASER:
		if(Layer == LAYER_GAME)
			return Localize("LASER: Gives you laser weapon with 10 charges.", "Editor Explanation");
		break;
	}
	if((Tile >= TILE_PUB_CREDITS1 && Tile <= TILE_PUB_CREDITS8) && Layer == LAYER_GAME)
		return Localize("CREDITS: Who designed the entities.", "Editor Explanation");
	else if((Tile == TILE_PUB_ENTITIES_OFF1 || Tile == TILE_PUB_ENTITIES_OFF2) && Layer == LAYER_GAME)
		return Localize("ENTITIES OFF SIGN: Informs people playing with entities about important marks, tips, information or text on the map.", "Editor Explanation");
	return nullptr;
}

const char *CEditor::Explain(EExplanation Explanation, int Tile, int Layer)
{
	switch(Explanation)
	{
	case EExplanation::DDNET:
		return ExplainDDNet(Tile, Layer);
	case EExplanation::FNG:
		return ExplainFNG(Tile, Layer);
	case EExplanation::RACE:
		return nullptr; // TODO: Explanations for Race
	case EExplanation::VANILLA:
		return ExplainVanilla(Tile, Layer);
	case EExplanation::BLOCKWORLDS:
		return nullptr; // TODO: Explanations for Blockworlds
	}
	dbg_assert(false, "logic error");
	return nullptr;
}
