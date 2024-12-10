from datatypes import Array, Float, Int, Pointer, String, Struct, TextureHandle

class Sound(Struct):
	def __init__(self, filename=""):
		Struct.__init__(self, "CDataSound")
		self.id = Int(0)
		self.filename = String(filename)

class SoundSet(Struct):
	def __init__(self, name="", files=()):
		Struct.__init__(self, "CDataSoundset")
		self.name = String(name)
		self.sounds = Array(Sound())
		self.last = Int(-1)
		for filename in files:
			self.sounds.Add(Sound(filename))

class Image(Struct):
	def __init__(self, name="", filename="", linear_mapping=0):
		Struct.__init__(self, "CDataImage")
		self.name = String(name)
		self.filename = String(filename)
		self.flag = Int(linear_mapping)
		self.id = TextureHandle()

class SpriteSet(Struct):
	def __init__(self, _name="", image=None, gridx=0, gridy=0):
		Struct.__init__(self, "CDataSpriteset")
		self.image = Pointer(Image, image) # TODO
		self.gridx = Int(gridx)
		self.gridy = Int(gridy)

class Sprite(Struct):
	def __init__(self, name="", Set=None, x=0, y=0, w=0, h=0):
		Struct.__init__(self, "CDataSprite")
		self.name = String(name)
		self.set = Pointer(SpriteSet, Set) # TODO
		self.x = Int(x)
		self.y = Int(y)
		self.w = Int(w)
		self.h = Int(h)

class Pickup(Struct):
	def __init__(self, name="", respawntime=15, spawndelay=0):
		Struct.__init__(self, "CDataPickupspec")
		self.name = String(name)
		self.respawntime = Int(respawntime)
		self.spawndelay = Int(spawndelay)

class AnimKeyframe(Struct):
	def __init__(self, time=0, x=0, y=0, angle=0):
		Struct.__init__(self, "CAnimKeyframe")
		self.time = Float(time)
		self.x = Float(x)
		self.y = Float(y)
		self.angle = Float(angle)

class AnimSequence(Struct):
	def __init__(self):
		Struct.__init__(self, "CAnimSequence")
		self.frames = Array(AnimKeyframe())

class Animation(Struct):
	def __init__(self, name=""):
		Struct.__init__(self, "CAnimation")
		self.name = String(name)
		self.body = AnimSequence()
		self.back_foot = AnimSequence()
		self.front_foot = AnimSequence()
		self.attach = AnimSequence()

class WeaponSpec(Struct):
	def __init__(self, cont=None, name=""):
		Struct.__init__(self, "CDataWeaponspec")
		self.name = String(name)
		self.sprite_body = Pointer(Sprite, Sprite())
		self.sprite_cursor = Pointer(Sprite, Sprite())
		self.sprite_proj = Pointer(Sprite, Sprite())
		self.sprite_muzzles = Array(Pointer(Sprite, Sprite()))
		self.visual_size = Int(96)

		self.firedelay = Int(500)
		self.maxammo = Int(10)
		self.ammoregentime = Int(0)
		self.damage = Int(1)

		self.offsetx = Float(0)
		self.offsety = Float(0)
		self.muzzleoffsetx = Float(0)
		self.muzzleoffsety = Float(0)
		self.muzzleduration = Float(5)

		# dig out sprites if we have a container
		if cont:
			for sprite in cont.sprites.items:
				if sprite.name.value == "weapon_"+name+"_body":
					self.sprite_body.Set(sprite)
				elif sprite.name.value == "weapon_"+name+"_cursor":
					self.sprite_cursor.Set(sprite)
				elif sprite.name.value == "weapon_"+name+"_proj":
					self.sprite_proj.Set(sprite)
				elif "weapon_"+name+"_muzzle" in sprite.name.value:
					self.sprite_muzzles.Add(Pointer(Sprite, sprite))

class Weapon_Hammer(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecHammer")
		self.base = Pointer(WeaponSpec, WeaponSpec())

class Weapon_Gun(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecGun")
		self.base = Pointer(WeaponSpec, WeaponSpec())
		self.curvature = Float(1.25)
		self.speed = Float(2200)
		self.lifetime = Float(2.0)

class Weapon_Shotgun(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecShotgun")
		self.base = Pointer(WeaponSpec, WeaponSpec())
		self.curvature = Float(1.25)
		self.speed = Float(2200)
		self.speeddiff = Float(0.8)
		self.lifetime = Float(0.25)

class Weapon_Grenade(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecGrenade")
		self.base = Pointer(WeaponSpec, WeaponSpec())
		self.curvature = Float(7.0)
		self.speed = Float(1000)
		self.lifetime = Float(2.0)

class Weapon_Laser(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecLaser")
		self.base = Pointer(WeaponSpec, WeaponSpec())
		self.reach = Float(800.0)
		self.bounce_delay = Int(150)
		self.bounce_num = Int(1)
		self.bounce_cost = Float(0)

class Weapon_Ninja(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecNinja")
		self.base = Pointer(WeaponSpec, WeaponSpec())
		self.duration = Int(15000)
		self.movetime = Int(200)
		self.velocity = Int(50)

class Weapons(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataWeaponspecs")
		self.hammer = Weapon_Hammer()
		self.gun = Weapon_Gun()
		self.shotgun = Weapon_Shotgun()
		self.grenade = Weapon_Grenade()
		self.laser = Weapon_Laser()
		self.ninja = Weapon_Ninja()
		self.id = Array(WeaponSpec())

class DataContainer(Struct):
	def __init__(self):
		Struct.__init__(self, "CDataContainer")
		self.sounds = Array(SoundSet())
		self.images = Array(Image())
		self.pickups = Array(Pickup())
		self.spritesets = Array(SpriteSet())
		self.sprites = Array(Sprite())
		self.animations = Array(Animation())
		self.weapons = Weapons()

def FileList(fmt, num):
	return [fmt%(x+1) for x in range(0,num)]

container = DataContainer()
container.sounds.Add(SoundSet("gun_fire", FileList("audio/wp_gun_fire-%02d.wv", 3)))
container.sounds.Add(SoundSet("shotgun_fire", FileList("audio/wp_shotty_fire-%02d.wv", 3)))

container.sounds.Add(SoundSet("grenade_fire", FileList("audio/wp_flump_launch-%02d.wv", 3)))
container.sounds.Add(SoundSet("hammer_fire", FileList("audio/wp_hammer_swing-%02d.wv", 3)))
container.sounds.Add(SoundSet("hammer_hit", FileList("audio/wp_hammer_hit-%02d.wv", 3)))
container.sounds.Add(SoundSet("ninja_fire", FileList("audio/wp_ninja_attack-%02d.wv", 3)))
container.sounds.Add(SoundSet("grenade_explode", FileList("audio/wp_flump_explo-%02d.wv", 3)))
container.sounds.Add(SoundSet("ninja_hit", FileList("audio/wp_ninja_hit-%02d.wv", 3)))
container.sounds.Add(SoundSet("laser_fire", FileList("audio/wp_laser_fire-%02d.wv", 3)))
container.sounds.Add(SoundSet("laser_bounce", FileList("audio/wp_laser_bnce-%02d.wv", 3)))
container.sounds.Add(SoundSet("weapon_switch", FileList("audio/wp_switch-%02d.wv", 3)))

container.sounds.Add(SoundSet("player_pain_short", FileList("audio/vo_teefault_pain_short-%02d.wv", 12)))
container.sounds.Add(SoundSet("player_pain_long", FileList("audio/vo_teefault_pain_long-%02d.wv", 2)))

container.sounds.Add(SoundSet("body_land", FileList("audio/foley_land-%02d.wv", 4)))
container.sounds.Add(SoundSet("player_airjump", FileList("audio/foley_dbljump-%02d.wv", 3)))
container.sounds.Add(SoundSet("player_jump", FileList("audio/foley_foot_left-%02d.wv", 4) + FileList("audio/foley_foot_right-%02d.wv", 4)))
container.sounds.Add(SoundSet("player_die", FileList("audio/foley_body_splat-%02d.wv", 3)))
container.sounds.Add(SoundSet("player_spawn", FileList("audio/vo_teefault_spawn-%02d.wv", 7)))
container.sounds.Add(SoundSet("player_skid", FileList("audio/sfx_skid-%02d.wv", 4)))
container.sounds.Add(SoundSet("tee_cry", FileList("audio/vo_teefault_cry-%02d.wv", 2)))

container.sounds.Add(SoundSet("hook_loop", FileList("audio/hook_loop-%02d.wv", 2)))

container.sounds.Add(SoundSet("hook_attach_ground", FileList("audio/hook_attach-%02d.wv", 3)))
container.sounds.Add(SoundSet("hook_attach_player", FileList("audio/foley_body_impact-%02d.wv", 3)))
container.sounds.Add(SoundSet("hook_noattach", FileList("audio/hook_noattach-%02d.wv", 2)))
container.sounds.Add(SoundSet("pickup_health", FileList("audio/sfx_pickup_hrt-%02d.wv", 2)))
container.sounds.Add(SoundSet("pickup_armor", FileList("audio/sfx_pickup_arm-%02d.wv", 4)))

container.sounds.Add(SoundSet("pickup_grenade", ["audio/sfx_pickup_launcher.wv"]))
container.sounds.Add(SoundSet("pickup_shotgun", ["audio/sfx_pickup_sg.wv"]))
container.sounds.Add(SoundSet("pickup_ninja", ["audio/sfx_pickup_ninja.wv"]))
container.sounds.Add(SoundSet("weapon_spawn", FileList("audio/sfx_spawn_wpn-%02d.wv", 3)))
container.sounds.Add(SoundSet("weapon_noammo", FileList("audio/wp_noammo-%02d.wv", 5)))

container.sounds.Add(SoundSet("hit", FileList("audio/sfx_hit_weak-%02d.wv", 2)))

container.sounds.Add(SoundSet("chat_server", ["audio/sfx_msg-server.wv"]))
container.sounds.Add(SoundSet("chat_client", ["audio/sfx_msg-client.wv"]))
container.sounds.Add(SoundSet("chat_highlight", ["audio/sfx_msg-highlight.wv"]))
container.sounds.Add(SoundSet("ctf_drop", ["audio/sfx_ctf_drop.wv"]))
container.sounds.Add(SoundSet("ctf_return", ["audio/sfx_ctf_rtn.wv"]))
container.sounds.Add(SoundSet("ctf_grab_pl", ["audio/sfx_ctf_grab_pl.wv"]))
container.sounds.Add(SoundSet("ctf_grab_en", ["audio/sfx_ctf_grab_en.wv"]))
container.sounds.Add(SoundSet("ctf_capture", ["audio/sfx_ctf_cap_pl.wv"]))

container.sounds.Add(SoundSet("menu", ["audio/music_menu.wv"]))

image_null = Image("null", "")
image_particles = Image("particles", "particles.png")
image_game = Image("game", "game.png")
image_emoticons = Image("emoticons", "emoticons.png")
image_speedup_arrow = Image("speedup_arrow", "editor/speed_arrow.png")
image_guibuttons = Image("guibuttons", "gui_buttons.png")
image_guiicons = Image("guiicons", "gui_icons.png")
image_arrow = Image("arrow", "arrow.png")
image_audio_source = Image("audio_source", "editor/audio_source.png")
image_strongweak = Image("strongweak", "strong_weak.png")
image_hud = Image("hud", "hud.png")
image_extras = Image("extras", "extras.png")

container.images.Add(image_null)
container.images.Add(image_game)
container.images.Add(image_particles)
container.images.Add(Image("cursor", "gui_cursor.png"))
container.images.Add(Image("banner", "gui_logo.png"))
container.images.Add(image_emoticons)
container.images.Add(Image("background_noise", "background_noise.png"))
container.images.Add(image_speedup_arrow)
container.images.Add(image_guibuttons)
container.images.Add(image_guiicons)
container.images.Add(image_arrow)
container.images.Add(image_audio_source)
container.images.Add(image_strongweak)
container.images.Add(image_hud)
container.images.Add(image_extras)
container.images.Add(Image("raceflag", "race_flag.png"))

container.pickups.Add(Pickup("health"))
container.pickups.Add(Pickup("armor"))
container.pickups.Add(Pickup("armor_shotgun"))
container.pickups.Add(Pickup("armor_grenade"))
container.pickups.Add(Pickup("armor_laser"))
container.pickups.Add(Pickup("armor_ninja"))
container.pickups.Add(Pickup("weapon"))
container.pickups.Add(Pickup("ninja", 90, 90))

set_particles = SpriteSet("particles", image_particles, 8, 8)
set_game = SpriteSet("game", image_game, 32, 16)
set_tee = SpriteSet("tee", image_null, 8, 4)
set_emoticons = SpriteSet("emoticons", image_emoticons, 4, 4)
set_speedup_arrow = SpriteSet("speedup_arrow", image_speedup_arrow, 1, 1)
set_guibuttons = SpriteSet("guibuttons", image_guibuttons, 12, 4)
set_guiicons = SpriteSet("guiicons", image_guiicons, 12, 2)
set_audio_source = SpriteSet("audio_source", image_audio_source, 1, 1)
set_strongweak = SpriteSet("strongweak", image_strongweak, 3, 1)
set_hud = SpriteSet("hud", image_hud, 16, 16)
set_extras = SpriteSet("extras", image_extras, 16, 16)

container.spritesets.Add(set_particles)
container.spritesets.Add(set_game)
container.spritesets.Add(set_tee)
container.spritesets.Add(set_emoticons)
container.spritesets.Add(set_speedup_arrow)
container.spritesets.Add(set_guibuttons)
container.spritesets.Add(set_guiicons)
container.spritesets.Add(set_audio_source)
container.spritesets.Add(set_strongweak)
container.spritesets.Add(set_hud)
container.spritesets.Add(set_extras)

container.sprites.Add(Sprite("part_slice", set_particles, 0,0,1,1))
container.sprites.Add(Sprite("part_ball", set_particles, 1,0,1,1))
container.sprites.Add(Sprite("part_splat01", set_particles, 2,0,1,1))
container.sprites.Add(Sprite("part_splat02", set_particles, 3,0,1,1))
container.sprites.Add(Sprite("part_splat03", set_particles, 4,0,1,1))

container.sprites.Add(Sprite("part_smoke", set_particles, 0,1,1,1))
container.sprites.Add(Sprite("part_shell", set_particles, 0,2,2,2))
container.sprites.Add(Sprite("part_expl01", set_particles, 0,4,4,4))
container.sprites.Add(Sprite("part_airjump", set_particles, 2,2,2,2))
container.sprites.Add(Sprite("part_hit01", set_particles, 4,1,2,2))

container.sprites.Add(Sprite("health_full", set_game, 21,0,2,2))
container.sprites.Add(Sprite("health_empty", set_game, 23,0,2,2))
container.sprites.Add(Sprite("armor_full", set_game, 21,2,2,2))
container.sprites.Add(Sprite("armor_empty", set_game, 23,2,2,2))

container.sprites.Add(Sprite("star1", set_game, 15,0,2,2))
container.sprites.Add(Sprite("star2", set_game, 17,0,2,2))
container.sprites.Add(Sprite("star3", set_game, 19,0,2,2))

container.sprites.Add(Sprite("part1", set_game, 6,0,1,1))
container.sprites.Add(Sprite("part2", set_game, 6,1,1,1))
container.sprites.Add(Sprite("part3", set_game, 7,0,1,1))
container.sprites.Add(Sprite("part4", set_game, 7,1,1,1))
container.sprites.Add(Sprite("part5", set_game, 8,0,1,1))
container.sprites.Add(Sprite("part6", set_game, 8,1,1,1))
container.sprites.Add(Sprite("part7", set_game, 9,0,2,2))
container.sprites.Add(Sprite("part8", set_game, 11,0,2,2))
container.sprites.Add(Sprite("part9", set_game, 13,0,2,2))

container.sprites.Add(Sprite("weapon_gun_body", set_game, 2,4,4,2))
container.sprites.Add(Sprite("weapon_gun_cursor", set_game, 0,4,2,2))
container.sprites.Add(Sprite("weapon_gun_proj", set_game, 6,4,2,2))
container.sprites.Add(Sprite("weapon_gun_muzzle1", set_game, 8,4,4,2))
container.sprites.Add(Sprite("weapon_gun_muzzle2", set_game, 12,4,4,2))
container.sprites.Add(Sprite("weapon_gun_muzzle3", set_game, 16,4,4,2))

container.sprites.Add(Sprite("weapon_shotgun_body", set_game, 2,6,8,2))
container.sprites.Add(Sprite("weapon_shotgun_cursor", set_game, 0,6,2,2))
container.sprites.Add(Sprite("weapon_shotgun_proj", set_game, 10,6,2,2))
container.sprites.Add(Sprite("weapon_shotgun_muzzle1", set_game, 12,6,4,2))
container.sprites.Add(Sprite("weapon_shotgun_muzzle2", set_game, 16,6,4,2))
container.sprites.Add(Sprite("weapon_shotgun_muzzle3", set_game, 20,6,4,2))

container.sprites.Add(Sprite("weapon_grenade_body", set_game, 2,8,7,2))
container.sprites.Add(Sprite("weapon_grenade_cursor", set_game, 0,8,2,2))
container.sprites.Add(Sprite("weapon_grenade_proj", set_game, 10,8,2,2))

container.sprites.Add(Sprite("weapon_hammer_body", set_game, 2,1,4,3))
container.sprites.Add(Sprite("weapon_hammer_cursor", set_game, 0,0,2,2))
container.sprites.Add(Sprite("weapon_hammer_proj", set_game, 0,0,0,0))

container.sprites.Add(Sprite("weapon_ninja_body", set_game, 2,10,8,2))
container.sprites.Add(Sprite("weapon_ninja_cursor", set_game, 0,10,2,2))
container.sprites.Add(Sprite("weapon_ninja_proj", set_game, 0,0,0,0))

container.sprites.Add(Sprite("weapon_laser_body", set_game, 2,12,7,3))
container.sprites.Add(Sprite("weapon_laser_cursor", set_game, 0,12,2,2))
container.sprites.Add(Sprite("weapon_laser_proj", set_game, 10,12,2,2))

container.sprites.Add(Sprite("hook_chain", set_game, 2,0,1,1))
container.sprites.Add(Sprite("hook_head", set_game, 3,0,2,1))

container.sprites.Add(Sprite("weapon_ninja_muzzle1", set_game, 25,0,7,4))
container.sprites.Add(Sprite("weapon_ninja_muzzle2", set_game, 25,4,7,4))
container.sprites.Add(Sprite("weapon_ninja_muzzle3", set_game, 25,8,7,4))

container.sprites.Add(Sprite("pickup_health", set_game, 10,2,2,2))
container.sprites.Add(Sprite("pickup_armor", set_game, 12,2,2,2))
container.sprites.Add(Sprite("pickup_hammer", set_game, 2,1,4,3))
container.sprites.Add(Sprite("pickup_gun", set_game, 2,4,4,2))
container.sprites.Add(Sprite("pickup_shotgun", set_game, 2,6,8,2))
container.sprites.Add(Sprite("pickup_grenade", set_game, 2,8,7,2))
container.sprites.Add(Sprite("pickup_laser", set_game, 2,12,7,3))
container.sprites.Add(Sprite("pickup_ninja", set_game, 2,10,8,2))
container.sprites.Add(Sprite("pickup_armor_shotgun", set_game, 15,2,2,2))
container.sprites.Add(Sprite("pickup_armor_grenade", set_game, 17,2,2,2))
container.sprites.Add(Sprite("pickup_armor_ninja", set_game, 10,10,2,2))
container.sprites.Add(Sprite("pickup_armor_laser", set_game, 19,2,2,2))

container.sprites.Add(Sprite("flag_blue", set_game, 12,8,4,8))
container.sprites.Add(Sprite("flag_red", set_game, 16,8,4,8))

container.sprites.Add(Sprite("tee_body", set_tee, 0,0,3,3))
container.sprites.Add(Sprite("tee_body_outline", set_tee, 3,0,3,3))
container.sprites.Add(Sprite("tee_foot", set_tee, 6,1,2,1))
container.sprites.Add(Sprite("tee_foot_outline", set_tee, 6,2,2,1))
container.sprites.Add(Sprite("tee_hand", set_tee, 6,0,1,1))
container.sprites.Add(Sprite("tee_hand_outline", set_tee, 7,0,1,1))
container.sprites.Add(Sprite("tee_eye_normal", set_tee, 2,3,1,1))
container.sprites.Add(Sprite("tee_eye_angry", set_tee, 3,3,1,1))
container.sprites.Add(Sprite("tee_eye_pain", set_tee, 4,3,1,1))
container.sprites.Add(Sprite("tee_eye_happy", set_tee, 5,3,1,1))
container.sprites.Add(Sprite("tee_eye_dead", set_tee, 6,3,1,1))
container.sprites.Add(Sprite("tee_eye_surprise", set_tee, 7,3,1,1))

container.sprites.Add(Sprite("oop", set_emoticons, 0, 0, 1, 1))
container.sprites.Add(Sprite("exclamation", set_emoticons, 1, 0, 1, 1))
container.sprites.Add(Sprite("hearts", set_emoticons, 2, 0, 1, 1))
container.sprites.Add(Sprite("drop", set_emoticons, 3, 0, 1, 1))
container.sprites.Add(Sprite("dotdot", set_emoticons, 0, 1, 1, 1))
container.sprites.Add(Sprite("music", set_emoticons, 1, 1, 1, 1))
container.sprites.Add(Sprite("sorry", set_emoticons, 2, 1, 1, 1))
container.sprites.Add(Sprite("ghost", set_emoticons, 3, 1, 1, 1))
container.sprites.Add(Sprite("sushi", set_emoticons, 0, 2, 1, 1))
container.sprites.Add(Sprite("splattee", set_emoticons, 1, 2, 1, 1))
container.sprites.Add(Sprite("deviltee", set_emoticons, 2, 2, 1, 1))
container.sprites.Add(Sprite("zomg", set_emoticons, 3, 2, 1, 1))
container.sprites.Add(Sprite("zzz", set_emoticons, 0, 3, 1, 1))
container.sprites.Add(Sprite("wtf", set_emoticons, 1, 3, 1, 1))
container.sprites.Add(Sprite("eyes", set_emoticons, 2, 3, 1, 1))
container.sprites.Add(Sprite("question", set_emoticons, 3, 3, 1, 1))

container.sprites.Add(Sprite("speedup_arrow", set_speedup_arrow, 0,0,1,1))

container.sprites.Add(Sprite("guibutton_off", set_guibuttons, 0,0,4,4))
container.sprites.Add(Sprite("guibutton_on", set_guibuttons, 4,0,4,4))
container.sprites.Add(Sprite("guibutton_hover", set_guibuttons, 8,0,4,4))

container.sprites.Add(Sprite("guiicon_mute", set_guiicons, 0,0,4,2))
container.sprites.Add(Sprite("guiicon_emoticon_mute", set_guiicons, 4,0,4,2))
container.sprites.Add(Sprite("guiicon_friend", set_guiicons, 8,0,4,2))

container.sprites.Add(Sprite("audio_source", set_audio_source, 0,0,1,1))

container.sprites.Add(Sprite("hook_strong", set_strongweak, 0,0,1,1))
container.sprites.Add(Sprite("hook_weak", set_strongweak, 1,0,1,1))
container.sprites.Add(Sprite("hook_icon", set_strongweak, 2,0,1,1))

container.sprites.Add(Sprite("hud_airjump", set_hud, 0,0,2,2))
container.sprites.Add(Sprite("hud_airjump_empty", set_hud, 2,0,2,2))
container.sprites.Add(Sprite("hud_solo", set_hud, 4,0,2,2))
container.sprites.Add(Sprite("hud_collision_disabled", set_hud, 6,0,2,2))
container.sprites.Add(Sprite("hud_endless_jump", set_hud, 8,0,2,2))
container.sprites.Add(Sprite("hud_endless_hook", set_hud, 10,0,2,2))
container.sprites.Add(Sprite("hud_jetpack", set_hud, 12,0,2,2))
container.sprites.Add(Sprite("hud_freeze_bar_full_left", set_hud, 0,2,1,1))
container.sprites.Add(Sprite("hud_freeze_bar_full", set_hud, 1,2,1,1))
container.sprites.Add(Sprite("hud_freeze_bar_empty", set_hud, 2,2,1,1))
container.sprites.Add(Sprite("hud_freeze_bar_empty_right", set_hud, 3,2,1,1))
container.sprites.Add(Sprite("hud_ninja_bar_full_left", set_hud, 0,3,1,1))
container.sprites.Add(Sprite("hud_ninja_bar_full", set_hud, 1,3,1,1))
container.sprites.Add(Sprite("hud_ninja_bar_empty", set_hud, 2,3,1,1))
container.sprites.Add(Sprite("hud_ninja_bar_empty_right", set_hud, 3,3,1,1))
container.sprites.Add(Sprite("hud_hook_hit_disabled", set_hud, 4,2,2,2))
container.sprites.Add(Sprite("hud_hammer_hit_disabled", set_hud, 6,2,2,2))
container.sprites.Add(Sprite("hud_shotgun_hit_disabled", set_hud, 8,2,2,2))
container.sprites.Add(Sprite("hud_grenade_hit_disabled", set_hud, 10,2,2,2))
container.sprites.Add(Sprite("hud_laser_hit_disabled", set_hud, 12,2,2,2))
container.sprites.Add(Sprite("hud_gun_hit_disabled", set_hud, 14,2,2,2))
container.sprites.Add(Sprite("hud_deep_frozen", set_hud, 10,4,2,2))
container.sprites.Add(Sprite("hud_live_frozen", set_hud, 12,4,2,2))
container.sprites.Add(Sprite("hud_teleport_grenade", set_hud, 4,4,2,2))
container.sprites.Add(Sprite("hud_teleport_gun", set_hud, 6,4,2,2))
container.sprites.Add(Sprite("hud_teleport_laser", set_hud, 8,4,2,2))
container.sprites.Add(Sprite("hud_practice_mode", set_hud, 4,6,2,2))
container.sprites.Add(Sprite("hud_dummy_hammer", set_hud, 6,6,2,2))
container.sprites.Add(Sprite("hud_dummy_copy", set_hud, 8,6,2,2))
container.sprites.Add(Sprite("hud_lock_mode", set_hud, 10,6,2,2))
container.sprites.Add(Sprite("hud_team0_mode", set_hud, 12,6,2,2))

container.sprites.Add(Sprite("part_snowflake", set_extras, 0,0,2,2))
container.sprites.Add(Sprite("part_sparkle", set_extras, 2,0,2,2))


anim = Animation("base")
anim.body.frames.Add(AnimKeyframe(0, 0, -4, 0))
anim.back_foot.frames.Add(AnimKeyframe(0, 0, 10, 0))
anim.front_foot.frames.Add(AnimKeyframe(0, 0, 10, 0))
container.animations.Add(anim)

anim = Animation("idle")
anim.back_foot.frames.Add(AnimKeyframe(0, -7, 0, 0))
anim.front_foot.frames.Add(AnimKeyframe(0, 7, 0, 0))
container.animations.Add(anim)

anim = Animation("inair")
anim.back_foot.frames.Add(AnimKeyframe(0, -3, 0, -0.1))
anim.front_foot.frames.Add(AnimKeyframe(0, 3, 0, -0.1))
container.animations.Add(anim)

anim = Animation("sit_left")
anim.body.frames.Add(AnimKeyframe(0, 0, 3, 0))
anim.back_foot.frames.Add(AnimKeyframe(0, -12, 0, 0.1))
anim.front_foot.frames.Add(AnimKeyframe(0, -8, 0, 0.1))
container.animations.Add(anim)

anim = Animation("sit_right")
anim.body.frames.Add(AnimKeyframe(0, 0, 3, 0))
anim.back_foot.frames.Add(AnimKeyframe(0, 12, 0, -0.1))
anim.front_foot.frames.Add(AnimKeyframe(0, 8, 0, -0.1))
container.animations.Add(anim)

anim = Animation("walk")
anim.body.frames.Add(AnimKeyframe(0.0, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.2, 0,-1, 0))
anim.body.frames.Add(AnimKeyframe(0.4, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.6, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.8, 0,-1, 0))
anim.body.frames.Add(AnimKeyframe(1.0, 0, 0, 0))

anim.back_foot.frames.Add(AnimKeyframe(0.0, 8, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(0.2, -8, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(0.4,-10,-4, 0.2))
anim.back_foot.frames.Add(AnimKeyframe(0.6, -8,-8, 0.3))
anim.back_foot.frames.Add(AnimKeyframe(0.8, 4,-4,-0.2))
anim.back_foot.frames.Add(AnimKeyframe(1.0, 8, 0, 0))

anim.front_foot.frames.Add(AnimKeyframe(0.0,-10,-4, 0.2))
anim.front_foot.frames.Add(AnimKeyframe(0.2, -8,-8, 0.3))
anim.front_foot.frames.Add(AnimKeyframe(0.4, 4,-4,-0.2))
anim.front_foot.frames.Add(AnimKeyframe(0.6, 8, 0, 0))
anim.front_foot.frames.Add(AnimKeyframe(0.8, 8, 0, 0))
anim.front_foot.frames.Add(AnimKeyframe(1.0,-10,-4, 0.2))
container.animations.Add(anim)

# the run_left animation is taken directly from run_right, only the x and rotate values are flipped,
# and each string is run backwards, to account for how it's played in game.
anim = Animation("run_left")
anim.body.frames.Add(AnimKeyframe(0.0, 0, -1, 0))
anim.body.frames.Add(AnimKeyframe(0.2, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.4, 0, -1, 0))
anim.body.frames.Add(AnimKeyframe(0.6, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.8, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(1.0, 0, -1, 0))

anim.back_foot.frames.Add(AnimKeyframe(0.0, 18, -8, -0.27))
anim.back_foot.frames.Add(AnimKeyframe(0.2, 6, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(0.4, -7, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(0.6, -13, -4.5, 0.05))
anim.back_foot.frames.Add(AnimKeyframe(0.8, 0, -8, -0.2))
anim.back_foot.frames.Add(AnimKeyframe(1.0, 18, -8, -0.27))

anim.front_foot.frames.Add(AnimKeyframe(0.0, -11, -2.5, 0.05))
anim.front_foot.frames.Add(AnimKeyframe(0.2, -14, -5, 0.1))
anim.front_foot.frames.Add(AnimKeyframe(0.4, 11, -8, -0.3))
anim.front_foot.frames.Add(AnimKeyframe(0.6, 18, -8, -0.27))
anim.front_foot.frames.Add(AnimKeyframe(0.8, 3, 0, 0))
anim.front_foot.frames.Add(AnimKeyframe(1.0, -11, -2.5, 0.05))
container.animations.Add(anim)

anim = Animation("run_right")
anim.body.frames.Add(AnimKeyframe(0.0, 0, -1, 0))
anim.body.frames.Add(AnimKeyframe(0.2, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.4, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(0.6, 0, -1, 0))
anim.body.frames.Add(AnimKeyframe(0.8, 0, 0, 0))
anim.body.frames.Add(AnimKeyframe(1.0, 0, -1, 0))

anim.back_foot.frames.Add(AnimKeyframe(0.0, -18, -8, 0.27))
anim.back_foot.frames.Add(AnimKeyframe(0.2, 0, -8, 0.2))
anim.back_foot.frames.Add(AnimKeyframe(0.4, 13, -4.5, -0.05))
anim.back_foot.frames.Add(AnimKeyframe(0.6, 7, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(0.8, -6, 0, 0))
anim.back_foot.frames.Add(AnimKeyframe(1.0, -18, -8, 0.27))

anim.front_foot.frames.Add(AnimKeyframe(0.0, 11, -2.5, -0.05))
anim.front_foot.frames.Add(AnimKeyframe(0.2, -3, 0, 0))
anim.front_foot.frames.Add(AnimKeyframe(0.4, -18, -8, 0.27))
anim.front_foot.frames.Add(AnimKeyframe(0.6, -11, -8, 0.3))
anim.front_foot.frames.Add(AnimKeyframe(0.8, 14, -5, -0.1))
anim.front_foot.frames.Add(AnimKeyframe(1.0, 11, -2.5, -0.05))
container.animations.Add(anim)

anim = Animation("hammer_swing")
anim.attach.frames.Add(AnimKeyframe(0.0, 0, 0, -0.10))
anim.attach.frames.Add(AnimKeyframe(0.3, 0, 0, 0.25))
anim.attach.frames.Add(AnimKeyframe(0.4, 0, 0, 0.30))
anim.attach.frames.Add(AnimKeyframe(0.5, 0, 0, 0.25))
anim.attach.frames.Add(AnimKeyframe(1.0, 0, 0, -0.10))
container.animations.Add(anim)

anim = Animation("ninja_swing")
anim.attach.frames.Add(AnimKeyframe(0.00, 0, 0, -0.25))
anim.attach.frames.Add(AnimKeyframe(0.10, 0, 0, -0.05))
anim.attach.frames.Add(AnimKeyframe(0.15, 0, 0, 0.35))
anim.attach.frames.Add(AnimKeyframe(0.42, 0, 0, 0.40))
anim.attach.frames.Add(AnimKeyframe(0.50, 0, 0, 0.35))
anim.attach.frames.Add(AnimKeyframe(1.00, 0, 0, -0.25))
container.animations.Add(anim)

weapon = WeaponSpec(container, "hammer")
weapon.firedelay.Set(125)
weapon.damage.Set(3)
weapon.visual_size.Set(96)
weapon.offsetx.Set(4)
weapon.offsety.Set(-20)
container.weapons.hammer.base.Set(weapon)
container.weapons.id.Add(weapon)

weapon = WeaponSpec(container, "gun")
weapon.firedelay.Set(125)
weapon.ammoregentime.Set(500)
weapon.visual_size.Set(64)
weapon.offsetx.Set(32)
weapon.offsety.Set(4)
# the number after the plus sign is the sprite scale, which is calculated for all sprites ( w / sqrt(w² * h²) ) of the additionally added x offset, which is added now,
# since the muzzle image is 32 pixels bigger, divided by 2, because a sprite's position is always at the center of the sprite image itself
# => the offset added, bcs the sprite is bigger now, but should not be shifted to the left
# => 96 / sqrt(64×64+96×96)  (the original sprite scale)
# => 64 × original sprite scale (the actual size of the sprite ingame see weapon.visual_size above)
# => (actual image sprite) / 3 (the new sprite is 128 instead of 96, so 4 / 3 times as big(bcs it should look the same as before, not scaled down because of this bigger number), so basically, 1 / 3 of the original size is added)
# => (new sprite width) / 2 (bcs the sprite is at center, only add half of that new extra width)
weapon.muzzleoffsetx.Set(50 + 8.8752)
weapon.muzzleoffsety.Set(6)
container.weapons.gun.base.Set(weapon)
container.weapons.id.Add(weapon)

weapon = WeaponSpec(container, "shotgun")
weapon.firedelay.Set(500)
weapon.visual_size.Set(96)
weapon.offsetx.Set(24)
weapon.offsety.Set(-2)
weapon.muzzleoffsetx.Set(70 + 13.3128) # see gun for the number after the plus sign
weapon.muzzleoffsety.Set(6)
container.weapons.shotgun.base.Set(weapon)
container.weapons.id.Add(weapon)

weapon = WeaponSpec(container, "grenade")
weapon.firedelay.Set(500) # TODO: fix this
weapon.visual_size.Set(96)
weapon.offsetx.Set(24)
weapon.offsety.Set(-2)
container.weapons.grenade.base.Set(weapon)
container.weapons.id.Add(weapon)

weapon = WeaponSpec(container, "laser")
weapon.firedelay.Set(800)
weapon.visual_size.Set(92)
weapon.damage.Set(5)
weapon.offsetx.Set(24)
weapon.offsety.Set(-2)
container.weapons.laser.base.Set(weapon)
container.weapons.id.Add(weapon)

weapon = WeaponSpec(container, "ninja")
weapon.firedelay.Set(800)
weapon.damage.Set(9)
weapon.visual_size.Set(96)
weapon.offsetx.Set(0)
weapon.offsety.Set(0)
weapon.muzzleoffsetx.Set(40)
weapon.muzzleoffsety.Set(-4)
container.weapons.ninja.base.Set(weapon)
container.weapons.id.Add(weapon)
