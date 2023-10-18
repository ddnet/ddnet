//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_PLAYER_H
#define DDNET_API_PLAYER_H

#include "Python.h"
#include "game/client/gameclient.h"
#include "api.h"
#include "api_vector2.h"
#include "api_tee.h"

using namespace std;

struct Player {
	PyObject_HEAD;
	int useCustomColor;
	int colorBody;
	int colorFeet;

	char name[16];
	char clan[12];
	int country;
	char skinName[64];
	int skinColor;
	int team;
	int emoticon;
	float emoticonStartFraction;
	int emoticonStartTick;
	bool isSolo;
	bool isJetpack;
	bool isCollisionDisabled;
	bool isEndlessHook;
	bool isEndlessJump;
	bool isHammerHitDisabled;
	bool isGrenadeHitDisabled;
	bool isLaserHitDisabled;
	bool isShotgunHitDisabled;
	bool isHookHitDisabled;
	bool isSuper;
	bool isHasTelegunGun;
	bool isHasTelegunGrenade;
	bool isHasTelegunLaser;
	int freezeEnd;
	bool isDeepFrozen;
	bool isLiveFrozen;

	float angle;
	bool isActive;
	bool isChatIgnore;
	bool isEmoticonIgnore;
	bool isFriend;
	bool isFoe;

	int authLevel;
	bool isAfk;
	bool isPaused;
	bool isSpec;

	Vector2 renderPos;
	bool isPredicted;
	bool isPredictedLocal;
	int64_t smoothStart[2];
	int64_t smoothLen[2];
	bool isSpecCharPresent;
	Vector2 specChar;

	Tee tee;
};

extern PyTypeObject PlayerType;

static PyMethodDef Player_methods[] = {
	{NULL}  /* Sentinel */
};

static int Player_init(Player *self, PyObject *args, PyObject *kwds)
{
	int id;

	if (!PyArg_ParseTuple(args, "i", &id))
		return -1;

	if (id < 0 || id > 63) {
		PyErr_SetString(PyExc_TypeError, "Expected int argument, with value between 0-63");

		return -1;
	}

	CGameClient::CClientData clientData = PythonAPI_GameClient->m_aClients[id];

	self->useCustomColor = clientData.m_UseCustomColor;
	self->colorBody = clientData.m_ColorBody;
	self->colorFeet = clientData.m_ColorFeet;

	strcpy(self->name, clientData.m_aName);
	strcpy(self->clan, clientData.m_aClan);
	self->country = clientData.m_Country;
	strcpy(self->skinName, clientData.m_aSkinName);
	self->skinColor = clientData.m_SkinColor;
	self->team = clientData.m_Team;
	self->emoticon = clientData.m_Emoticon;
	self->emoticonStartFraction = clientData.m_EmoticonStartFraction;
	self->emoticonStartTick = clientData.m_EmoticonStartTick;
	self->isSolo = clientData.m_Solo;
	self->isJetpack = clientData.m_Jetpack;
	self->isCollisionDisabled = clientData.m_CollisionDisabled;
	self->isEndlessHook = clientData.m_EndlessHook;
	self->isEndlessJump = clientData.m_EndlessJump;
	self->isHammerHitDisabled = clientData.m_HammerHitDisabled;
	self->isGrenadeHitDisabled = clientData.m_GrenadeHitDisabled;
	self->isLaserHitDisabled = clientData.m_LaserHitDisabled;
	self->isShotgunHitDisabled = clientData.m_ShotgunHitDisabled;
	self->isHookHitDisabled = clientData.m_HookHitDisabled;
	self->isSuper = clientData.m_Super;
	self->isHasTelegunGun = clientData.m_HasTelegunGun;
	self->isHasTelegunGrenade = clientData.m_HasTelegunGrenade;
	self->isHasTelegunLaser = clientData.m_HasTelegunLaser;
	self->freezeEnd = clientData.m_FreezeEnd;
	self->isDeepFrozen = clientData.m_DeepFrozen;
	self->isLiveFrozen = clientData.m_LiveFrozen;

	self->angle = clientData.m_Angle;
	self->isActive = clientData.m_Active;
	self->isChatIgnore = clientData.m_ChatIgnore;
	self->isEmoticonIgnore = clientData.m_EmoticonIgnore;
	self->isFriend = clientData.m_Friend;
	self->isFoe = clientData.m_Foe;

	self->authLevel = clientData.m_AuthLevel;
	self->isAfk = clientData.m_Afk;
	self->isPaused = clientData.m_Paused;
	self->isSpec = clientData.m_Spec;

	self->renderPos = Vector2(clientData.m_RenderPos);
	self->isPredicted = clientData.m_IsPredicted;
	self->isPredictedLocal = clientData.m_IsPredictedLocal;
	self->smoothStart[0] = clientData.m_aSmoothStart[0];
	self->smoothStart[1] = clientData.m_aSmoothStart[1];
	self->smoothLen[0] = clientData.m_aSmoothLen[0];
	self->smoothLen[1] = clientData.m_aSmoothLen[1];
	self->isSpecCharPresent = clientData.m_SpecCharPresent;
	self->specChar = clientData.m_SpecChar;

	self->tee.pos = Vector2(clientData.m_Predicted.m_Pos);
	self->tee.vel = Vector2(clientData.m_Predicted.m_Vel);
	self->tee.hookPos = Vector2(clientData.m_Predicted.m_HookPos);
	self->tee.hookDir = Vector2(clientData.m_Predicted.m_HookDir);
	self->tee.hookTeleBase = Vector2(clientData.m_Predicted.m_HookTeleBase);
	self->tee.hookTick = clientData.m_Predicted.m_HookTick;
	self->tee.hookState = clientData.m_Predicted.m_HookState;
	self->tee.hookedPlayer = clientData.m_Predicted.m_HookedPlayer;
	self->tee.activeWeapon = clientData.m_Predicted.m_ActiveWeapon;
	self->tee.isNewHook = clientData.m_Predicted.m_NewHook;
	self->tee.jumped = clientData.m_Predicted.m_Jumped;
	self->tee.jumpedTotal = clientData.m_Predicted.m_JumpedTotal;
	self->tee.jumps = clientData.m_Predicted.m_Jumps;
	self->tee.direction = clientData.m_Predicted.m_Direction;
	self->tee.angle = clientData.m_Predicted.m_Angle;
	self->tee.triggeredEvents = clientData.m_Predicted.m_TriggeredEvents;
	self->tee.id = clientData.m_Predicted.m_Id;
	self->tee.isReset = clientData.m_Predicted.m_Reset;
	self->tee.lastVel = Vector2(clientData.m_Predicted.m_LastVel);
	self->tee.colliding = clientData.m_Predicted.m_Colliding;
	self->tee.isLeftWall = clientData.m_Predicted.m_LeftWall;
	self->tee.isSolo = clientData.m_Predicted.m_Solo;
	self->tee.isJetpack = clientData.m_Predicted.m_Jetpack;
	self->tee.isCollisionDisabled = clientData.m_Predicted.m_CollisionDisabled;
	self->tee.isEndlessHook = clientData.m_Predicted.m_EndlessHook;
	self->tee.isEndlessJump = clientData.m_Predicted.m_EndlessJump;
	self->tee.isHammerHitDisabled = clientData.m_Predicted.m_HammerHitDisabled;
	self->tee.isGrenadeHitDisabled = clientData.m_Predicted.m_GrenadeHitDisabled;
	self->tee.isLaserHitDisabled = clientData.m_Predicted.m_LaserHitDisabled;
	self->tee.isShotgunHitDisabled = clientData.m_Predicted.m_ShotgunHitDisabled;
	self->tee.isHookHitDisabled = clientData.m_Predicted.m_HookHitDisabled;
	self->tee.isSuper = clientData.m_Predicted.m_Super;
	self->tee.hasTelegunGun = clientData.m_Predicted.m_HasTelegunGun;
	self->tee.hasTelegunGrenade = clientData.m_Predicted.m_HasTelegunGrenade;
	self->tee.hasTelegunLaser = clientData.m_Predicted.m_HasTelegunLaser;
	self->tee.freezeStart = clientData.m_Predicted.m_FreezeStart;
	self->tee.freezeEnd = clientData.m_Predicted.m_FreezeEnd;
	self->tee.isInFreeze = clientData.m_Predicted.m_IsInFreeze;
	self->tee.isDeepFrozen = clientData.m_Predicted.m_DeepFrozen;
	self->tee.isLiveFrozen = clientData.m_Predicted.m_LiveFrozen;

	return 0;
}

static void Player_dealloc(Player* self)
{
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Player_getuseCustomColor(Player* self, void* closure)
{
	return Py_BuildValue("i", self->useCustomColor);
}

static PyObject* Player_getcolorBody(Player* self, void* closure)
{
	return Py_BuildValue("i", self->colorBody);
}

static PyObject* Player_getcolorFeet(Player* self, void* closure)
{
	return Py_BuildValue("i", self->colorFeet);
}

static PyObject* Player_getname(Player* self, void* closure)
{
	return Py_BuildValue("s", self->name);
}

static PyObject* Player_getclan(Player* self, void* closure)
{
	return Py_BuildValue("s", self->clan);
}

static PyObject* Player_getcountry(Player* self, void* closure)
{
	return Py_BuildValue("i", self->country);
}

static PyObject* Player_getskinName(Player* self, void* closure)
{
	return Py_BuildValue("s", self->skinName);
}

static PyObject* Player_getskinColor(Player* self, void* closure)
{
	return Py_BuildValue("i", self->skinColor);
}

static PyObject* Player_getteam(Player* self, void* closure)
{
	return Py_BuildValue("i", self->team);
}

static PyObject* Player_getemoticon(Player* self, void* closure)
{
	return Py_BuildValue("i", self->emoticon);
}

static PyObject* Player_getemoticonStartFraction(Player* self, void* closure)
{
	return Py_BuildValue("d", (double) self->emoticonStartFraction);
}

static PyObject* Player_getemoticonStartTick(Player* self, void* closure)
{
	return Py_BuildValue("i", self->emoticonStartTick);
}

static PyObject* Player_getisSolo(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isSolo);
}

static PyObject* Player_getisJetpack(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isJetpack);
}

static PyObject* Player_getisCollisionDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isCollisionDisabled);
}

static PyObject* Player_getisEndlessHook(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isEndlessHook);
}

static PyObject* Player_getisEndlessJump(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isEndlessJump);
}

static PyObject* Player_getisHammerHitDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isHammerHitDisabled);
}

static PyObject* Player_getisGrenadeHitDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isGrenadeHitDisabled);
}

static PyObject* Player_getisLaserHitDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isLaserHitDisabled);
}

static PyObject* Player_getisShotgunHitDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isShotgunHitDisabled);
}

static PyObject* Player_getisHookHitDisabled(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isHookHitDisabled);
}

static PyObject* Player_getisSuper(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isSuper);
}

static PyObject* Player_getisHasTelegunGun(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isHasTelegunGun);
}

static PyObject* Player_getisHasTelegunGrenade(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isHasTelegunGrenade);
}

static PyObject* Player_getisHasTelegunLaser(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isHasTelegunLaser);
}

static PyObject* Player_getfreezeEnd(Player* self, void* closure)
{
	return Py_BuildValue("i", self->freezeEnd);
}

static PyObject* Player_getisDeepFrozen(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isDeepFrozen);
}

static PyObject* Player_getisLiveFrozen(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isLiveFrozen);
}

static PyObject* Player_getangle(Player* self, void* closure)
{
	return Py_BuildValue("d", (double) self->angle);
}

static PyObject* Player_getisActive(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isActive);
}

static PyObject* Player_getisChatIgnore(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isChatIgnore);
}

static PyObject* Player_getisEmoticonIgnore(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isEmoticonIgnore);
}

static PyObject* Player_getisFriend(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isFriend);
}

static PyObject* Player_getisFoe(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isFoe);
}

static PyObject* Player_getauthLevel(Player* self, void* closure)
{
	return Py_BuildValue("i", self->authLevel);
}

static PyObject* Player_getisAfk(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isAfk);
}

static PyObject* Player_getisPaused(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isPaused);
}

static PyObject* Player_getisSpec(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isSpec);
}

static PyObject* Player_getrenderPos(Player* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->renderPos.x;
	vector->y = self->renderPos.y;

	return (PyObject *) vector;
}

static PyObject* Player_getisPredicted(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isPredicted);
}

static PyObject* Player_getisPredictedLocal(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isPredictedLocal);
}

static PyObject* Player_getsmoothStart(Player* self, void* closure)
{
	return Py_BuildValue("(LL)", self->smoothStart[0], self->smoothStart[1]);
}

static PyObject* Player_getsmoothLen(Player* self, void* closure)
{
	return Py_BuildValue("(LL)", self->smoothLen[0], self->smoothLen[1]);
}

static PyObject* Player_getisSpecCharPresent(Player* self, void* closure)
{
	return Py_BuildValue("b", self->isSpecCharPresent);
}

static PyObject* Player_getspecChar(Player* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->specChar.x;
	vector->y = self->specChar.y;

	return (PyObject *) vector;
}

static PyObject* Player_gettee(Player* self, void* closure)
{
	Tee *tee = (Tee *)PyObject_New(Tee, &TeeType);

	tee->pos = self->tee.pos;
	tee->vel = self->tee.vel;
	tee->hookPos = self->tee.hookPos;
	tee->hookDir = self->tee.hookDir;
	tee->hookTeleBase = self->tee.hookTeleBase;
	tee->hookTick = self->tee.hookTick;
	tee->hookState = self->tee.hookState;
	tee->hookedPlayer = self->tee.hookedPlayer;
	tee->activeWeapon = self->tee.activeWeapon;
	tee->isNewHook = self->tee.isNewHook;
	tee->jumped = self->tee.jumped;
	tee->jumpedTotal = self->tee.jumpedTotal;
	tee->jumps = self->tee.jumps;
	tee->direction = self->tee.direction;
	tee->angle = self->tee.angle;
	tee->triggeredEvents = self->tee.triggeredEvents;
	tee->id = self->tee.id;
	tee->isReset = self->tee.isReset;
	tee->lastVel = self->tee.lastVel;
	tee->colliding = self->tee.colliding;
	tee->isLeftWall = self->tee.isLeftWall;
	tee->isSolo = self->tee.isSolo;
	tee->isJetpack = self->tee.isJetpack;
	tee->isCollisionDisabled = self->tee.isCollisionDisabled;
	tee->isEndlessHook = self->tee.isEndlessHook;
	tee->isEndlessJump = self->tee.isEndlessJump;
	tee->isHammerHitDisabled = self->tee.isHammerHitDisabled;
	tee->isGrenadeHitDisabled = self->tee.isGrenadeHitDisabled;
	tee->isLaserHitDisabled = self->tee.isLaserHitDisabled;
	tee->isShotgunHitDisabled = self->tee.isShotgunHitDisabled;
	tee->isHookHitDisabled = self->tee.isHookHitDisabled;
	tee->isSuper = self->tee.isSuper;
	tee->hasTelegunGun = self->tee.hasTelegunGun;
	tee->hasTelegunGrenade = self->tee.hasTelegunGrenade;
	tee->hasTelegunLaser = self->tee.hasTelegunLaser;
	tee->freezeStart = self->tee.freezeStart;
	tee->freezeEnd = self->tee.freezeEnd;
	tee->isInFreeze = self->tee.isInFreeze;
	tee->isDeepFrozen = self->tee.isDeepFrozen;
	tee->isLiveFrozen = self->tee.isLiveFrozen;

	return (PyObject *) tee;
}

static PyGetSetDef Player_getseters[] = {
	{"useCustomColor", (getter) Player_getuseCustomColor, NULL, "useCustomColor (integer)", NULL},
	{"colorBody", (getter) Player_getcolorBody, NULL, "colorBody (integer)", NULL},
	{"colorFeet", (getter) Player_getcolorFeet, NULL, "colorFeet (integer)", NULL},
	{"name", (getter) Player_getname, NULL, "name (string)", NULL},
	{"clan", (getter) Player_getclan, NULL, "clan (string)", NULL},
	{"country", (getter) Player_getcountry, NULL, "country (integer)", NULL},
	{"skinName", (getter) Player_getskinName, NULL, "skinName (string)", NULL},
	{"skinColor", (getter) Player_getskinColor, NULL, "skinColor (integer)", NULL},
	{"team", (getter) Player_getteam, NULL, "team (integer)", NULL},
	{"emoticon", (getter) Player_getemoticon, NULL, "emoticon (integer)", NULL},
	{"emoticonStartFraction", (getter) Player_getemoticonStartFraction, NULL, "emoticonStartFraction (double)", NULL},
	{"emoticonStartTick", (getter) Player_getemoticonStartTick, NULL, "emoticonStartTick (integer)", NULL},
	{"isSolo", (getter) Player_getisSolo, NULL, "isSolo (boolean)", NULL},
	{"isJetpack", (getter) Player_getisJetpack, NULL, "isJetpack (boolean)", NULL},
	{"isCollisionDisabled", (getter) Player_getisCollisionDisabled, NULL, "isCollisionDisabled (boolean)", NULL},
	{"isEndlessHook", (getter) Player_getisEndlessHook, NULL, "isEndlessHook (boolean)", NULL},
	{"isEndlessJump", (getter) Player_getisEndlessJump, NULL, "isEndlessJump (boolean)", NULL},
	{"isHammerHitDisabled", (getter) Player_getisHammerHitDisabled, NULL, "isHammerHitDisabled (boolean)", NULL},
	{"isGrenadeHitDisabled", (getter) Player_getisGrenadeHitDisabled, NULL, "isGrenadeHitDisabled (boolean)", NULL},
	{"isLaserHitDisabled", (getter) Player_getisLaserHitDisabled, NULL, "isLaserHitDisabled (boolean)", NULL},
	{"isShotgunHitDisabled", (getter) Player_getisShotgunHitDisabled, NULL, "isShotgunHitDisabled (boolean)", NULL},
	{"isHookHitDisabled", (getter) Player_getisHookHitDisabled, NULL, "isHookHitDisabled (boolean)", NULL},
	{"isSuper", (getter) Player_getisSuper, NULL, "isSuper (boolean)", NULL},
	{"isHasTelegunGun", (getter) Player_getisHasTelegunGun, NULL, "isHasTelegunGun (boolean)", NULL},
	{"isHasTelegunGrenade", (getter) Player_getisHasTelegunGrenade, NULL, "isHasTelegunGrenade (boolean)", NULL},
	{"isHasTelegunLaser", (getter) Player_getisHasTelegunLaser, NULL, "isHasTelegunLaser (boolean)", NULL},
	{"freezeEnd", (getter) Player_getfreezeEnd, NULL, "freezeEnd (integer)", NULL},
	{"isDeepFrozen", (getter) Player_getisDeepFrozen, NULL, "isDeepFrozen (boolean)", NULL},
	{"isLiveFrozen", (getter) Player_getisLiveFrozen, NULL, "isLiveFrozen (boolean)", NULL},
	{"angle", (getter) Player_getangle, NULL, "angle (double)", NULL},
	{"isActive", (getter) Player_getisActive, NULL, "isActive (boolean)", NULL},
	{"isChatIgnore", (getter) Player_getisChatIgnore, NULL, "isChatIgnore (boolean)", NULL},
	{"isEmoticonIgnore", (getter) Player_getisEmoticonIgnore, NULL, "isEmoticonIgnore (boolean)", NULL},
	{"isFriend", (getter) Player_getisFriend, NULL, "isFriend (boolean)", NULL},
	{"isFoe", (getter) Player_getisFoe, NULL, "isFoe (boolean)", NULL},
	{"authLevel", (getter) Player_getauthLevel, NULL, "authLevel (integer)", NULL},
	{"isAfk", (getter) Player_getisAfk, NULL, "isAfk (boolean)", NULL},
	{"isPaused", (getter) Player_getisPaused, NULL, "isPaused (boolean)", NULL},
	{"isSpec", (getter) Player_getisSpec, NULL, "isSpec (boolean)", NULL},
	{"renderPos", (getter) Player_getrenderPos, NULL, "renderPos (Vector2)", NULL},
	{"isPredicted", (getter) Player_getisPredicted, NULL, "isPredicted (boolean)", NULL},
	{"isPredictedLocal", (getter) Player_getisPredictedLocal, NULL, "isPredictedLocal (boolean)", NULL},
	{"smoothStart", (getter) Player_getsmoothStart, NULL, "smoothStart (pair of long int)", NULL},
	{"smoothLen", (getter) Player_getsmoothLen, NULL, "smoothLen (pair of long int)", NULL},
	{"isSpecCharPresent", (getter) Player_getisSpecCharPresent, NULL, "isSpecCharPresent (boolean)", NULL},
	{"specChar", (getter) Player_getspecChar, NULL, "specChar (Vector2)", NULL},
	{"tee", (getter) Player_gettee, NULL, "tee (Tee)", NULL},
	{NULL}  /* Sentinel */
};

static PyObject* Player_str(Player* self)
{
	char buf[4096];

	PyObject* name_str = PyUnicode_FromString(self->name);
	PyObject* clan_str = PyUnicode_FromString(self->clan);
	PyObject* skinName_str = PyUnicode_FromString(self->skinName);
	PyObject* renderPos_str_obj = Vector2_str(&self->renderPos);
	PyObject* specChar_str_obj = Vector2_str(&self->specChar);
	PyObject* tee_str_obj = Tee_str(&self->tee);

	const char *renderPos_str = PyUnicode_AsUTF8(renderPos_str_obj);
	const char *tee_str = PyUnicode_AsUTF8(tee_str_obj);
	const char *specChar_str = PyUnicode_AsUTF8(specChar_str_obj);

	if (!renderPos_str || !tee_str || !specChar_str) {
		Py_XDECREF(renderPos_str_obj);
		Py_XDECREF(tee_str_obj);
		Py_XDECREF(specChar_str_obj);
		return NULL;
	}

	sprintf(
		buf,
		"Player(\n"
		"useCustomColor: %d,\n"
		"colorBody: %d,\n"
		"colorFeet: %d,\n"
		"name: %s,\n"
		"clan: %s,\n"
		"country: %d,\n"
		"skinName: %s,\n"
		"skinColor: %d,\n"
		"team: %d,\n"
		"emoticon: %d,\n"
		"emoticonStartFraction: %.2f,\n"
		"emoticonStartTick: %d,\n"
		"isSolo: %s,\n"
		"isJetpack: %s,\n"
		"isCollisionDisabled: %s,\n"
		"isEndlessHook: %s,\n"
		"isEndlessJump: %s,\n"
		"isHammerHitDisabled: %s,\n"
		"isGrenadeHitDisabled: %s,\n"
		"isLaserHitDisabled: %s,\n"
		"isShotgunHitDisabled: %s,\n"
		"isHookHitDisabled: %s,\n"
		"isSuper: %s,\n"
		"isHasTelegunGun: %s,\n"
		"isHasTelegunGrenade: %s,\n"
		"isHasTelegunLaser: %s,\n"
		"freezeEnd: %d,\n"
		"isDeepFrozen: %s,\n"
		"isLiveFrozen: %s,\n"
		"angle: %.2f,\n"
		"isActive: %s,\n"
		"isChatIgnore: %s,\n"
		"isEmoticonIgnore: %s,\n"
		"isFriend: %s,\n"
		"isFoe: %s,\n"
		"authLevel: %d,\n"
		"isAfk: %s,\n"
		"isPaused: %s,\n"
		"isSpec: %s,\n"
		"renderPos: %s,\n"
		"isPredicted: %s,\n"
		"isPredictedLocal: %s,\n"
		"isSpecCharPresent: %s,\n"
		"specChar: %s,\n"
		"tee: %s\n"
		")",
		self->useCustomColor, self->colorBody, self->colorFeet, PyUnicode_AsUTF8(name_str), PyUnicode_AsUTF8(clan_str),
		self->country, PyUnicode_AsUTF8(skinName_str), self->skinColor, self->team, self->emoticon,
		self->emoticonStartFraction, self->emoticonStartTick, self->isSolo ? "true" : "false",
		self->isJetpack ? "true" : "false", self->isCollisionDisabled ? "true" : "false",
		self->isEndlessHook ? "true" : "false", self->isEndlessJump ? "true" : "false",
		self->isHammerHitDisabled ? "true" : "false", self->isGrenadeHitDisabled ? "true" : "false",
		self->isLaserHitDisabled ? "true" : "false", self->isShotgunHitDisabled ? "true" : "false",
		self->isHookHitDisabled ? "true" : "false", self->isSuper ? "true" : "false",
		self->isHasTelegunGun ? "true" : "false", self->isHasTelegunGrenade ? "true" : "false",
		self->isHasTelegunLaser ? "true" : "false", self->freezeEnd, self->isDeepFrozen ? "true" : "false",
		self->isLiveFrozen ? "true" : "false", self->angle, self->isActive ? "true" : "false",
		self->isChatIgnore ? "true" : "false", self->isEmoticonIgnore ? "true" : "false",
		self->isFriend ? "true" : "false", self->isFoe ? "true" : "false", self->authLevel,
		self->isAfk ? "true" : "false", self->isPaused ? "true" : "false", self->isSpec ? "true" : "false",
		renderPos_str, self->isSpecCharPresent ? "true" : "false", self->isPredicted ? "true" : "false",
		self->isPredictedLocal ? "true" : "false", specChar_str, tee_str
	);

	Py_DECREF(renderPos_str_obj);
	Py_DECREF(tee_str_obj);
	Py_DECREF(name_str);
	Py_DECREF(clan_str);
	Py_DECREF(skinName_str);
	Py_DECREF(specChar_str);

	return PyUnicode_FromString(buf);
}

static PyTypeObject PlayerType = {
	{ PyObject_HEAD_INIT(NULL) 0, },
	"API.Player",                /* tp_name */
	sizeof(Player),              /* tp_basicsize */
	0,                            /* tp_itemsize */
	(destructor)Player_dealloc,  /* tp_dealloc */
	0,                            /* tp_print */
	0,                            /* tp_getattr */
	0,                            /* tp_setattr */
	0,                            /* tp_compare */
	0,                            /* tp_repr */
	0,               /* tp_as_number */
	0,                            /* tp_as_sequence */
	0,                            /* tp_as_mapping */
	0,                            /* tp_hash */
	0,                            /* tp_call */
	(reprfunc)Player_str,                            /* tp_str */
	0,                            /* tp_getattro */
	0,                            /* tp_setattro */
	0,                            /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,           /* tp_flags */
	"Player",           /* tp_doc */
	0,                            /* tp_traverse */
	0,                            /* tp_clear */
	0,                            /* tp_richcompare */
	0,                            /* tp_weaklistoffset */
	0,                            /* tp_iter */
	0,                            /* tp_iternext */
	Player_methods,              /* tp_methods */
	0,                            /* tp_members */
	Player_getseters,                            /* tp_getset */
	0,                            /* tp_base */
	0,                            /* tp_dict */
	0,                            /* tp_descr_get */
	0,                            /* tp_descr_set */
	0,                            /* tp_dictoffset */
	(initproc)Player_init,      /* tp_init */
	0,                            /* tp_alloc */
	PyType_GenericNew,            /* tp_new */
};

#endif // DDNET_API_PLAYER_H
