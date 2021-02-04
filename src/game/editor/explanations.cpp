#include "editor.h"

// Explanations by Lady Saavik
const char *CEditor::Explain(int Tile, int Layer)
{
	switch(Tile)
	{
	case TILE_AIR:
		return "EMPTY: Can be used as an eraser.";
	case TILE_SOLID:
		if(Layer == LAYER_GAME)
			return "HOOKABLE: It's possible to hook and collide with it.";
		break;
	case TILE_DEATH:
		if(Layer == LAYER_GAME)
			return "KILL: Kills the tee.";
		break;
	case TILE_NOHOOK:
		if(Layer == LAYER_GAME)
			return "UNHOOKABLE: It's not possible to hook it, but can collide with it.";
		break;
	case TILE_NOLASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "LASER BLOCKER: Doesn't let DRAGGIN & SPINNING LASER and PLASMA TURRET reach tees through it.";
		break;
	case TILE_THROUGH_CUT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOKTHROUGH: Shortcut for new hookthrough.";
		break;
	case TILE_THROUGH_ALL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOKTHROUGH: Combined with a collision tile is new hookthrough, otherwise stops hooks, from all directions.";
		break;
	case TILE_THROUGH_DIR:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOKTHROUGH: Combined with a collision tile is new hookthrough, otherwise stops hook, from one direction.";
		break;
	case TILE_THROUGH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOKTHROUGH: Combined with (UN)HOOKABLE tiles, allows to hook through the walls.";
		break;
	case TILE_JUMP:
		if(Layer == LAYER_SWITCH)
			return "JUMP: Sets defined amount of jumps (default is 2).";
		break;
	case TILE_FREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "FREEZE: Freezes tees for 3 seconds.";
		if(Layer == LAYER_SWITCH)
			return "FREEZE: Freezes tees for defined amount of seconds.";
		break;
	case TILE_UNFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "UNFREEZE: Unfreezes tees immediately.";
		break;
	case TILE_TELEINEVIL:
		if(Layer == LAYER_TELE)
			return "RED TELEPORT: After falling into this tile, tees appear on TO with the same number. Speed and hooks are reset.";
		break;
	case TILE_DFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DEEP FREEZE: Permanent freeze. Only UNDEEP tile can cancel this effect.";
		break;
	case TILE_DUNFREEZE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "UNDEEP: Removes DEEP FREEZE effect.";
		break;
	case TILE_TELEINWEAPON:
		if(Layer == LAYER_TELE)
			return "WEAPON TELEPORT: Teleports bullets shot into it to TELEPORT TO, where it comes out. Direction, angle and length are kept.";
		break;
	case TILE_TELEINHOOK:
		if(Layer == LAYER_TELE)
			return "HOOK TELEPORT: Teleports hooks entering into it to TELEPORT TO, where it comes out. Direction, angle and length are kept.";
		break;
	case TILE_WALLJUMP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "WALLJUMP: Placed next to a wall. Enables climbing up the wall.";
		break;
	case TILE_EHOOK_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "ENDLESS HOOK: Activates endless hook.";
		break;
	case TILE_EHOOK_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "ENDLESS HOOK OFF: Deactivates endless hook.";
		break;
	case TILE_HIT_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HIT OTHERS: You can hit others.";
		if(Layer == LAYER_SWITCH)
			return "HIT OTHERS: You can activate hitting others for single weapons, using delay number to select which.";
		break;
	case TILE_HIT_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HIT OTHERS: You can't hit others.";
		if(Layer == LAYER_SWITCH)
			return "HIT OTHERS: You can deactivate hitting others for single weapons, using delay number to select which.";
		break;
	case TILE_SOLO_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SOLO: You are now in a solo part.";
		break;
	case TILE_SOLO_DISABLE: // also TILE_SWITCHTIMEDOPEN
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SOLO: You are now out of the solo part.";
		if(Layer == LAYER_SWITCH)
			return "TIME SWITCH: Activates switch (e.g. closes door) with the same number for a set amount of seconds.";
		break;
	case TILE_SWITCHTIMEDCLOSE:
		if(Layer == LAYER_SWITCH)
			return "TIME SWITCH: Deactivates switch (e.g. opens door) with the same number for a set amount of seconds.";
		break;
	case TILE_SWITCHOPEN:
		if(Layer == LAYER_SWITCH)
			return "SWITCH: Activates switch (e.g. closes door) with the same number.";
		break;
	case TILE_SWITCHCLOSE:
		if(Layer == LAYER_SWITCH)
			return "SWITCH: Deactivates switch (e.g. opens door) with the same number.";
		break;
	case TILE_TELEIN:
		if(Layer == LAYER_TELE)
			return "BLUE TELEPORT: After falling into this tile, tees appear on TO with the same number. Speed and hook are kept.";
		break;
	case TILE_TELEOUT:
		if(Layer == LAYER_TELE)
			return "TELEPORT TO: Destination tile for FROMs, WEAPON & HOOK TELEPORTs with the same numbers.";
		break;
	case TILE_BOOST:
		if(Layer == LAYER_SPEEDUP)
			return "SPEEDUP: Gives tee defined speed. Arrow shows direction and angle.";
		break;
	case TILE_TELECHECK:
		if(Layer == LAYER_TELE)
			return "CHECKPOINT TELEPORT: After having touched this tile, any CFRM will teleport you to CTO with the same number.";
		break;
	case TILE_TELECHECKOUT:
		if(Layer == LAYER_TELE)
			return "CHECKPOINT TELEPORT TO: Tees will appear here after touching TELEPORT CHECKPOINT with the same number and falling into CFROM TELEPORT.";
		break;
	case TILE_TELECHECKIN:
		if(Layer == LAYER_TELE)
			return "BLUE CHECKPOINT TELEPORT: Sends tees to CTO with the same number as the last touched TELEPORT CHECKPOINT. Speed and hook are kept.";
		break;
	case TILE_REFILL_JUMPS:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "REFILL JUMPS: Restores all jumps.";
		break;
	case TILE_START:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "START: Starts counting your race time.";
		break;
	case TILE_FINISH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "FINISH: End of race.";
		break;
	case TILE_STOP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "STOPPER: You can hook and shoot through it. You can't go through it against the arrow.";
		break;
	case TILE_STOPS:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "STOPPER: You can hook and shoot through it. You can't go through it against the arrows.";
		break;
	case TILE_STOPA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "STOPPER: You can hook and shoot through it. You can't go through it.";
		break;
	case TILE_TELECHECKINEVIL:
		if(Layer == LAYER_TELE)
			return "RED CHECKPOINT TELEPORT: Send tees to CTO with the same number as the last touched TELEPORT CHECKPOINT. Speed and hook are reset.";
		break;
	case TILE_CP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SPEEDER: Causes weapons, SHIELD, HEART and SPINNING LASER to move slowly.";
		break;
	case TILE_CP_F:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SPEEDER: Causes weapons, SHIELD, HEART and SPINNING LASER to move quickly.";
		break;
	case TILE_TUNE:
		if(Layer == LAYER_TUNE)
			return "TUNE ZONE: Area where defined tunes work.";
		break;
	case TILE_OLDLASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "GLOBAL OLD SHOTGUN: Shotgun drags others always towards the shooter, even after having bounced. Shooter can't hit themselves. Place only one tile somewhere on the map.";
		break;
	case TILE_NPC:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "GLOBAL COLLISION OFF: Nobody can collide with others. Place only one tile somewhere on the map.";
		break;
	case TILE_EHOOK:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "GLOBAL ENDLESS HOOK ON: Everyone has endless hook. Place only one tile somewhere on the map.";
		break;
	case TILE_NOHIT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "GLOBAL HIT OTHERS OFF: Nobody can hit others. Place only one tile somewhere on the map.";
		break;
	case TILE_NPH:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "GLOBAL HOOK OTHERS OFF: Nobody can hook others. Place only one tile somewhere on the map.";
		break;
	case TILE_UNLOCK_TEAM:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "UNLOCK TEAM: Forces team to be unlocked so that team doesn't get killed when one dies.";
		break;
	case TILE_ADD_TIME:
		if(Layer == LAYER_SWITCH)
			return "PENALTY: Adds time to your current race time. Opposite of BONUS.";
		break;
	case TILE_NPC_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "COLLISION OFF: You can't collide with others.";
		break;
	case TILE_UNLIMITED_JUMPS_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SUPER JUMP OFF: You don't have unlimited air jumps.";
		break;
	case TILE_JETPACK_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "JETPACK OFF: You lose your jetpack gun.";
		break;
	case TILE_NPH_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOK OTHERS OFF: You can't hook others.";
		break;
	case TILE_SUBTRACT_TIME:
		if(Layer == LAYER_SWITCH)
			return "BONUS: Subtracts time from your current race time. Opposite of PENALTY.";
		break;
	case TILE_NPC_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "COLLISION: You can collide with others.";
		break;
	case TILE_UNLIMITED_JUMPS_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SUPER JUMP: You have unlimited air jumps.";
		break;
	case TILE_JETPACK_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "JETPACK: You have a jetpack gun.";
		break;
	case TILE_NPH_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "HOOK OTHERS: You can hook others.";
		break;
	case TILE_CREDITS_1:
	case TILE_CREDITS_2:
	case TILE_CREDITS_3:
	case TILE_CREDITS_4:
	case TILE_CREDITS_5:
	case TILE_CREDITS_6:
	case TILE_CREDITS_7:
	case TILE_CREDITS_8:
		return "CREDITS: Who designed the entities.";
	case TILE_ENTITIES_OFF_1:
	case TILE_ENTITIES_OFF_2:
		return "ENTITIES OFF SIGN: Informs people playing with entities about important marks, tips, information or text on the map.";
	// Entities
	case ENTITY_OFFSET + ENTITY_SPAWN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SPAWN: Here tees will appear after joining the game or dying somewhere on the map.";
		break;
	case ENTITY_OFFSET + ENTITY_SPAWN_RED:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SPAWN: Red team members spawn here, same as normal spawn in DDRace.";
		break;
	case ENTITY_OFFSET + ENTITY_SPAWN_BLUE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "SPAWN: Blue team members spawn here, same as normal spawn in DDRace.";
		break;
	case ENTITY_OFFSET + ENTITY_FLAGSTAND_RED:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "FLAG: Not used in DDRace. Place where red team flag is.";
		break;
	case ENTITY_OFFSET + ENTITY_FLAGSTAND_BLUE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "FLAG: Not used in DDRace. Place where blue team flag is.";
		break;
	case ENTITY_OFFSET + ENTITY_ARMOR_1:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SHIELD: Takes all weapons (except hammer and pistol) away.";
		break;
	case ENTITY_OFFSET + ENTITY_HEALTH_1:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "HEART: Works like a FREEZE tile. Freezes tees for 3 seconds by default.";
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_SHOTGUN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SHOTGUN: Drags the tees towards it. Bounces off the walls.";
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_GRENADE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "GRENADE LAUNCHER: Throws exploding bullets. Also known as rocket.";
		break;
	case ENTITY_OFFSET + ENTITY_POWERUP_NINJA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "NINJA: Makes you invisible in the darkest nights.";
		break;
	case ENTITY_OFFSET + ENTITY_WEAPON_LASER:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER: Unfreezes hit tee. Bounces off the walls. Also known as laser.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_FAST_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, fast.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_NORMAL_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, medium speed.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SLOW_CCW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Counter-Clockwise, slow.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_STOP:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "NON-SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SLOW_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, slow.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_NORMAL_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, medium speed.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_FAST_CW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "SPINNING LASER: Tile where freezing laser (made with LASER LENGTH) begins. Clockwise, fast.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_SHORT:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 3 tiles long.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_MEDIUM:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 6 tiles long.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_LONG:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH: Put next to DOOR or SPINNING LASER, makes it 9 tiles long.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_SLOW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, slow.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, medium speed.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_C_FAST:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Lengthen, fast.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_SLOW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, slow.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, medium speed.";
		break;
	case ENTITY_OFFSET + ENTITY_LASER_O_FAST:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "LASER LENGTH CHANGE: Put next to LASER LENGTH, causes it to length and shorten constantly. Works only on (NON-)SPINNING LASER, not on DOOR. Shorten, fast.";
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "PLASMA TURRET: Shoots plasma bullets at the closest tee. They explode on an obstactle they hit (wall or tee).";
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAF:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "PLASMA TURRET: Shoots plasma bullets that work like FREEZE at the closest tee.";
		break;
	case ENTITY_OFFSET + ENTITY_PLASMA:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "PLASMA TURRET: Shoots plasma bullets that work like FREEZE at the closest tee. They also explode on an obstactly they hit (wall or tee).";
		break;
	case ENTITY_OFFSET + ENTITY_PLASMAU:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "PLASMA TURRET: Shoots plasma bullets that work like UNFREEZE at the closest tee.";
		break;
	case ENTITY_OFFSET + ENTITY_CRAZY_SHOTGUN_EX:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "EXPLODING BULLET: Bounces off the walls with explosion. Touching the bullet works like FREEZE tile (freezes for 3 seconds by default).";
		break;
	case ENTITY_OFFSET + ENTITY_CRAZY_SHOTGUN:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "EXPLODING BULLET: Bounces off the walls without explosion. Touching the bullet works like FREEZE tile (freezes for 3 seconds by default).";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_WEAK:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Weak.";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_NORMAL:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Medium strength.";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_STRONG:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can't reach tees through walls and LASER BLOCKER. Strong.";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_WEAK_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Weak.";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_NORMAL_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Medium strength.";
		break;
	case ENTITY_OFFSET + ENTITY_DRAGGER_STRONG_NW:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DRAGGING LASER: Grabs and attracts the closest tee to it. Can reach tees through walls but not through LASER BLOCKER. Strong.";
		break;
	case ENTITY_OFFSET + ENTITY_DOOR:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT || Layer == LAYER_SWITCH)
			return "DOOR: Combined with LASER LENGTH creates doors. Doesn't allow to go through it (only with NINJA).";
		break;
	case TILE_TELE_GUN_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELEGUN: Turn gun on as telegun weapon.";
		break;
	case TILE_TELE_GUN_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELEGUN OFF: Turn gun off as telegun weapon.";
		break;
	case TILE_ALLOW_TELE_GUN:
		if(Layer == LAYER_FRONT)
			return "TELEGUN: Place on top of a collision tile, activates a spot to teleport to, cancels movement.";
		if(Layer == LAYER_SWITCH)
			return "TELEGUN: Place on top of a collision tile, activates a spot to teleport to, cancels movement, for single weapons, using delay number to select which.";
		break;
	case TILE_ALLOW_BLUE_TELE_GUN:
		if(Layer == LAYER_FRONT)
			return "TELEGUN: Place on top of a collision tile, activates a spot to teleport to, preserves movement.";
		if(Layer == LAYER_SWITCH)
			return "TELEGUN: Place on top of a collision tile, activates a spot to teleport to, preserves movement, for single weapons, using delay number to select which.";
		break;
	case TILE_TELE_GRENADE_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELEGRENADE: Turn grenade on as telegun weapon.";
		break;
	case TILE_TELE_GRENADE_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELEGRENADE OFF: Turn grenade off as telegun weapon.";
		break;
	case TILE_TELE_LASER_ENABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELELASER: Turn laser on as telegun weapon.";
		break;
	case TILE_TELE_LASER_DISABLE:
		if(Layer == LAYER_GAME || Layer == LAYER_FRONT)
			return "TELELASER OFF: Turn laser off as telegun weapon.";
		break;
	}
	if(Tile >= TILE_CHECKPOINT_FIRST && Tile <= TILE_CHECKPOINT_LAST && (Layer == LAYER_GAME || Layer == LAYER_FRONT))
		return "TIME CHECKPOINT: Compares your current race time with your record to show you whether you are running faster or slower.";
	return "";
}
