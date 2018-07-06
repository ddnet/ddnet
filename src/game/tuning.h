/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_TUNING_H
#define GAME_TUNING_H
#undef GAME_TUNING_H // this file will be included several times

// physics tuning
MACRO_TUNING_PARAM(GroundControlSpeed, ground_control_speed, 10.0f, 0.0f, 100.0f, "Max speed the tee can get on ground")
MACRO_TUNING_PARAM(GroundControlAccel, ground_control_accel, 100.0f / TicksPerSecond, 0.0f, 100.0f, "Acceleration speed on the ground")
MACRO_TUNING_PARAM(GroundFriction, ground_friction, 0.5f, 0.0f, 100.0f, "Friction on the ground")
MACRO_TUNING_PARAM(GroundJumpImpulse, ground_jump_impulse, 13.2f, 0.0f, 100.0f, "Impulse when jumping on ground")
MACRO_TUNING_PARAM(AirJumpImpulse, air_jump_impulse, 12.0f, 0.0f, 100.0f, "Impulse when jumping in air")
MACRO_TUNING_PARAM(AirControlSpeed, air_control_speed, 250.0f / TicksPerSecond, 0.0f, 100.0f, "Max speed the tee can get in the air")
MACRO_TUNING_PARAM(AirControlAccel, air_control_accel, 1.5f, 0.0f, 100.0f, "Acceleration speed in air")
MACRO_TUNING_PARAM(AirFriction, air_friction, 0.95f, 0.0f, 1.0f, "Friction in the air")
MACRO_TUNING_PARAM(HookLength, hook_length, 380.0f, 0.0f, 1000.0f, "Length of the hook")
MACRO_TUNING_PARAM(HookFireSpeed, hook_fire_speed, 80.0f, 0.0f, 1000.0f, "How fast the hook is fired")
MACRO_TUNING_PARAM(HookDragAccel, hook_drag_accel, 3.0f, 0.0f, 100.0f, "Acceleration when hook is stuck")
MACRO_TUNING_PARAM(HookDragSpeed, hook_drag_speed, 15.0f, 0.0f, 100.0f, "Drag speed of the hook")
MACRO_TUNING_PARAM(Gravity, gravity, 0.5f, 0.0f, 10.0f, "Gravity of the teeworld")

MACRO_TUNING_PARAM(VelrampStart, velramp_start, 550, 0, 1000, "Velocity ramp start")
MACRO_TUNING_PARAM(VelrampRange, velramp_range, 2000, 0, 10000, "Velocity ramp range")
MACRO_TUNING_PARAM(VelrampCurvature, velramp_curvature, 1.4f, 0.0f, 100.0f, "Velocity ramp curvature")

// weapon tuning
MACRO_TUNING_PARAM(GunCurvature, gun_curvature, 1.25f, 0.0f, 100.0f, "Gun curvature")
MACRO_TUNING_PARAM(GunSpeed, gun_speed, 2200.0f, 0.0f, 10000.0f, "Gun speed")
MACRO_TUNING_PARAM(GunLifetime, gun_lifetime, 2.0f, 0.0f, 100.0f, "Gun lifetime")

MACRO_TUNING_PARAM(ShotgunCurvature, shotgun_curvature, 1.25f, 0.0f, 100.0f, "Shotgun curvature")
MACRO_TUNING_PARAM(ShotgunSpeed, shotgun_speed, 2750.0f, 0.0f, 10000.0f, "Shotgun speed")
MACRO_TUNING_PARAM(ShotgunSpeeddiff, shotgun_speeddiff, 0.8f, 0.0f, 100.0f, "(UNUSED) Speed difference between shotgun bullets")
MACRO_TUNING_PARAM(ShotgunLifetime, shotgun_lifetime, 0.20f, 0.0f, 1000.0f, "Shotgun lifetime")

MACRO_TUNING_PARAM(GrenadeCurvature, grenade_curvature, 7.0f, 0.0f, 100.0f, "Grenade curvature")
MACRO_TUNING_PARAM(GrenadeSpeed, grenade_speed, 1000.0f, 0.0f, 10000.0f, "Grenade speed")
MACRO_TUNING_PARAM(GrenadeLifetime, grenade_lifetime, 2.0f, 0.0f, 1000.0f, "Grenade lifetime")

MACRO_TUNING_PARAM(LaserReach, laser_reach, 800.0f, 0.0f, 1000.0f, "How long the laser can reach")
MACRO_TUNING_PARAM(LaserBounceDelay, laser_bounce_delay, 150, 0, 1000, "When bouncing, stop the laser this long")
MACRO_TUNING_PARAM(LaserBounceNum, laser_bounce_num, 1000, 0, 10000, "How many times the laser can bounce")
MACRO_TUNING_PARAM(LaserBounceCost, laser_bounce_cost, 0, 0, 1000, "Remove this much from reach when laser is bouncing")
MACRO_TUNING_PARAM(LaserDamage, laser_damage, 5, 0, 100, "Laser damage")

MACRO_TUNING_PARAM(PlayerCollision, player_collision, 1, 0, 1, "Enable player collisions")
MACRO_TUNING_PARAM(PlayerHooking, player_hooking, 1, 0, 1, "Enable player vs player hooking")

//ddnet tuning
MACRO_TUNING_PARAM(JetpackStrength, jetpack_strength, 400.0f, 0.0f, 1000.0f, "Jetpack pistol strength")
MACRO_TUNING_PARAM(ShotgunStrength, shotgun_strength, 10.0f, 0.0f, 100.0f, "Shotgun pull strength")
MACRO_TUNING_PARAM(ExplosionStrength, explosion_strength, 6.0f, 0.0f, 100.0f, "Explosion strength (grenade for example)")
MACRO_TUNING_PARAM(HammerStrength, hammer_strength, 1.0f, 0.0f, 100.0f, "Hammer strength")
MACRO_TUNING_PARAM(HookDuration, hook_duration, 1.25f, 0.0f, 100.0f, "Hook duration")

MACRO_TUNING_PARAM(HammerFireDelay, hammer_fire_delay, 125, 0, 1000, "Delay of hammering")
MACRO_TUNING_PARAM(GunFireDelay, gun_fire_delay, 125, 0, 1000, "Delay of firing gun" )
MACRO_TUNING_PARAM(ShotgunFireDelay, shotgun_fire_delay, 500, 0, 1000, "Delay of firing shotgun")
MACRO_TUNING_PARAM(GrenadeFireDelay, grenade_fire_delay, 500, 0, 1000, "Delay of firing grenade")
MACRO_TUNING_PARAM(LaserFireDelay, laser_fire_delay, 800, 0, 1000, "Delay of firing laser rifle")
MACRO_TUNING_PARAM(NinjaFireDelay, ninja_fire_delay, 800, 0, 1000, "Delay of firing ninja")
#endif
