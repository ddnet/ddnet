//
// Created by danii on 18.09.2023.
//

#ifndef DDNET_API_TEE_H
#define DDNET_API_TEE_H

#include "Python.h"
#include "game/client/gameclient.h"
#include "api.h"
#include "api_vector2.h"
#include "api_player.h"

using namespace std;

typedef struct {
	PyObject_HEAD;

	Vector2 pos;
	Vector2 vel;

	Vector2 hookPos;
	Vector2 hookDir;
	Vector2 hookTeleBase;
	int hookTick;
	int hookState;
	int hookedPlayer;

	int activeWeapon;

	bool isNewHook;

	int jumped;
	// m_JumpedTotal counts the jumps performed in the air
	int jumpedTotal;
	int jumps;

	int direction;
	int angle;

	int triggeredEvents;

	// DDRace
	int id;
	bool isReset;

	Vector2 lastVel;
	int colliding;
	bool isLeftWall;

	// DDNet Character
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
	bool hasTelegunGun;
	bool hasTelegunGrenade;
	bool hasTelegunLaser;
	int freezeStart;
	int freezeEnd;
	bool isInFreeze;
	bool isDeepFrozen;
	bool isLiveFrozen;
} Tee;

extern PyTypeObject TeeType;

static PyMethodDef Tee_methods[] = {
	{NULL}  /* Sentinel */
};

static int Tee_init(Tee *self, PyObject *args, PyObject *kwds)
{
	return 0;
}

static PyObject* Tee_getpos(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->pos.x;
	vector->y = self->pos.y;

	return (PyObject *) vector;
}

static PyObject* Tee_getvel(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->vel.x;
	vector->y = self->vel.y;

	return (PyObject *) vector;
}

static PyObject* Tee_gethookPos(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->hookPos.x;
	vector->y = self->hookPos.y;

	return (PyObject *) vector;
}

static PyObject* Tee_gethookDir(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->hookDir.x;
	vector->y = self->hookDir.y;

	return (PyObject *) vector;
}

static PyObject* Tee_gethookTeleBase(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->hookTeleBase.x;
	vector->y = self->hookTeleBase.y;

	return (PyObject *) vector;
}

static PyObject* Tee_gethookTick(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->hookTick);
}

static PyObject* Tee_gethookState(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->hookState);
}

static PyObject* Tee_gethookedPlayer(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->hookedPlayer);
}

static PyObject* Tee_getactiveWeapon(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->activeWeapon);
}

static PyObject* Tee_getisNewHook(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isNewHook);
}

static PyObject* Tee_getjumped(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->jumped);
}

static PyObject* Tee_getjumpedTotal(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->jumpedTotal);
}

static PyObject* Tee_getjumps(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->jumps);
}

static PyObject* Tee_getdirection(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->direction);
}

static PyObject* Tee_getangle(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->angle);
}

static PyObject* Tee_gettriggeredEvents(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->triggeredEvents);
}

static PyObject* Tee_getid(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->id);
}

static PyObject* Tee_getisReset(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isReset);
}

static PyObject* Tee_getlastVel(Tee* self, void* closure)
{
	Vector2 *vector = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	vector->x = self->lastVel.x;
	vector->y = self->lastVel.y;

	return (PyObject *) vector;
}

static PyObject* Tee_getcolliding(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->colliding);
}

static PyObject* Tee_getisLeftWall(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isLeftWall);
}

static PyObject* Tee_getisSolo(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isSolo);
}

static PyObject* Tee_getisJetpack(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isJetpack);
}

static PyObject* Tee_getisCollisionDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isCollisionDisabled);
}

static PyObject* Tee_getisEndlessHook(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isEndlessHook);
}

static PyObject* Tee_getisEndlessJump(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isEndlessJump);
}

static PyObject* Tee_getisHammerHitDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isHammerHitDisabled);
}

static PyObject* Tee_getisGrenadeHitDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isGrenadeHitDisabled);
}

static PyObject* Tee_getisLaserHitDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isLaserHitDisabled);
}

static PyObject* Tee_getisShotgunHitDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isShotgunHitDisabled);
}

static PyObject* Tee_getisHookHitDisabled(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isHookHitDisabled);
}

static PyObject* Tee_getisSuper(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isSuper);
}

static PyObject* Tee_gethasTelegunGun(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->hasTelegunGun);
}

static PyObject* Tee_gethasTelegunGrenade(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->hasTelegunGrenade);
}

static PyObject* Tee_gethasTelegunLaser(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->hasTelegunLaser);
}

static PyObject* Tee_getfreezeStart(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->freezeStart);
}

static PyObject* Tee_getfreezeEnd(Tee* self, void* closure)
{
	return Py_BuildValue("i", self->freezeEnd);
}

static PyObject* Tee_getisInFreeze(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isInFreeze);
}

static PyObject* Tee_getisDeepFrozen(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isDeepFrozen);
}

static PyObject* Tee_getisLiveFrozen(Tee* self, void* closure)
{
	return Py_BuildValue("b", self->isLiveFrozen);
}

static void Tee_dealloc(Tee* self)
{
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyGetSetDef Tee_getseters[] = {
	{"pos", (getter) Tee_getpos, NULL, "Position of the player (Vector2)", NULL},
	{"vel", (getter) Tee_getvel, NULL, "Velocity of the player (Vector2)", NULL},
	{"hookPos", (getter) Tee_gethookPos, NULL, "Hook position (Vector2)", NULL},
	{"hookDir", (getter) Tee_gethookDir, NULL, "Direction of the hook (Vector2)", NULL},
	{"hookTeleBase", (getter) Tee_gethookTeleBase, NULL, "Base position of the tele hook (Vector2)", NULL},
	{"hookTick", (getter) Tee_gethookTick, NULL, "Hook tick count (integer)", NULL},
	{"hookState", (getter) Tee_gethookState, NULL, "State of the hook (integer)", NULL},
	{"hookedPlayer", (getter) Tee_gethookedPlayer, NULL, "Hooked player id (integer)", NULL},
	{"activeWeapon", (getter) Tee_getactiveWeapon, NULL, "Active weapon id (integer)", NULL},
	{"isNewHook", (getter) Tee_getisNewHook, NULL, "Check if it is a new hook or not (boolean)", NULL},
	{"jumped", (getter) Tee_getjumped, NULL, "Check if player jumped (integer)", NULL},
	{"jumpedTotal", (getter) Tee_getjumpedTotal, NULL, "Total jumps made by player (integer)", NULL},
	{"jumps", (getter) Tee_getjumps, NULL, "Number of jumps thats left for player (integer)", NULL},
	{"direction", (getter) Tee_getdirection, NULL, "Direction in which player is facing (integer)", NULL},
	{"angle", (getter) Tee_getangle, NULL, "Angle which player is facing (integer)", NULL},
	{"triggeredEvents", (getter) Tee_gettriggeredEvents, NULL, "Events triggered by player (integer)", NULL},
	{"id", (getter) Tee_getid, NULL, "Player id (integer)", NULL},
	{"isReset", (getter) Tee_getisReset, NULL, "Check if player is resetted or not (boolean)", NULL},
	{"lastVel", (getter) Tee_getlastVel, NULL, "Last velocity of player (Vector2)", NULL},
	{"colliding", (getter) Tee_getcolliding, NULL, "Check if player is colliding (integer)", NULL},
	{"isLeftWall", (getter) Tee_getisLeftWall, NULL, "Check if player is touching left wall (boolean)", NULL},
	{"isSolo", (getter) Tee_getisSolo, NULL, "Check if player is solo or not (boolean)", NULL},
	{"isJetpack", (getter) Tee_getisJetpack, NULL, "Check if jetpack is activated or not (boolean)", NULL},
	{"isCollisionDisabled", (getter) Tee_getisCollisionDisabled, NULL, "Check if collision is disabled or not (boolean)", NULL},
	{"isEndlessHook", (getter) Tee_getisEndlessHook, NULL, "Check if endless hook is activated or not (boolean)", NULL},
	{"isEndlessJump", (getter) Tee_getisEndlessJump, NULL, "Check if endless jump is activated or not (boolean)", NULL},
	{"isHammerHitDisabled", (getter) Tee_getisHammerHitDisabled, NULL, "Check if hammer hit is disabled or not (boolean)", NULL},
	{"isGrenadeHitDisabled", (getter) Tee_getisGrenadeHitDisabled, NULL, "Check if grenade hit is disabled or not (boolean)", NULL},
	{"isLaserHitDisabled", (getter) Tee_getisLaserHitDisabled, NULL, "Check if laser hit is disabled or not (boolean)", NULL},
	{"isShotgunHitDisabled", (getter) Tee_getisShotgunHitDisabled, NULL, "Check if shotgun hit is disabled or not (boolean)", NULL},
	{"isHookHitDisabled", (getter) Tee_getisHookHitDisabled, NULL, "Check if hook hit is disabled or not (boolean)", NULL},
	{"isSuper", (getter) Tee_getisSuper, NULL, "Check if player is super or not (boolean)", NULL},
	{"hasTelegunGun", (getter) Tee_gethasTelegunGun, NULL, "Check if player has tele gun or not (boolean)", NULL},
	{"hasTelegunGrenade", (getter) Tee_gethasTelegunGrenade, NULL, "Check if player has tele grenade or not (boolean)", NULL},
	{"hasTelegunLaser", (getter) Tee_gethasTelegunLaser, NULL, "Check if player has tele laser or not (boolean)", NULL},
	{"freezeStart", (getter) Tee_getfreezeStart, NULL, "Freeze start tick count (integer)", NULL},
	{"freezeEnd", (getter) Tee_getfreezeEnd, NULL, "Freeze end tick count (integer)", NULL},
	{"isInFreeze", (getter) Tee_getisInFreeze, NULL, "Check if player is in freeze or not (boolean)", NULL},
	{"isDeepFrozen", (getter) Tee_getisDeepFrozen, NULL, "Check if player is deeply frozen or not (boolean)", NULL},
	{"isLiveFrozen", (getter) Tee_getisLiveFrozen, NULL, "Check if player is live frozen or not (boolean)", NULL},
	{NULL}  /* Sentinel */
};

static PyObject* Tee_str(Tee* self)
{
	// Буфер для строковых представлений полей struct.
	char buf[2048];

	// Создание строковых представлений для обьектов типа Vector2.
	PyObject* pos_str_obj = Vector2_str(&(self->pos));
	PyObject* vel_str_obj = Vector2_str(&(self->vel));
	PyObject* hookPos_str_obj = Vector2_str(&(self->hookPos));
	PyObject* hookDir_str_obj = Vector2_str(&(self->hookDir));
	PyObject* hookTeleBase_str_obj = Vector2_str(&(self->hookTeleBase));
	PyObject* lastVel_str_obj = Vector2_str(&(self->lastVel));

	// Получение указателей на C-строки.
	const char *pos_str = PyUnicode_AsUTF8(pos_str_obj);
	const char *vel_str = PyUnicode_AsUTF8(vel_str_obj);
	const char *hookPos_str = PyUnicode_AsUTF8(hookPos_str_obj);
	const char *hookDir_str = PyUnicode_AsUTF8(hookDir_str_obj);
	const char *hookTeleBase_str = PyUnicode_AsUTF8(hookTeleBase_str_obj);
	const char *lastVel_str = PyUnicode_AsUTF8(lastVel_str_obj);

	// Проверка получения всех указателей на строки.
	if (!pos_str || !vel_str || !hookPos_str || !hookDir_str || !hookTeleBase_str || !lastVel_str) {
		// Освобождение всех PyObject, если не удалось получить строки.
		Py_XDECREF(pos_str_obj);
		Py_XDECREF(vel_str_obj);
		Py_XDECREF(hookPos_str_obj);
		Py_XDECREF(hookDir_str_obj);
		Py_XDECREF(hookTeleBase_str_obj);
		Py_XDECREF(lastVel_str_obj);
		return NULL;
	}

	sprintf(
		buf,
		"Tee(\n"
			"	pos: %s,\n"
			"	vel: %s,\n"
			"	hookPos: %s,\n"
			"	hookDir: %s,\n"
			"	hookTeleBase: %s,\n"
			"	hookTick: %d,\n"
			"	hookState: %d,\n"
			"	hookedPlayer: %d,\n"
			"	activeWeapon: %d,\n"
			"	isNewHook: %s,\n"
			"	jumped: %d,\n"
			"	jumpedTotal: %d,\n"
			"	jumps: %d,\n"
			"	direction: %d,\n"
			"	angle: %d,\n"
			"	triggeredEvents: %d,\n"
			"	id: %d,\n"
			"	isReset: %s,\n"
			"	lastVel: %s,\n"
			"	colliding: %d,\n"
			"	isLeftWall: %s,\n"
			"	isSolo: %s,\n"
			"	isJetpack: %s,\n"
			"	isCollisionDisabled: %s,\n"
			"	isEndlessHook: %s,\n"
			"	isEndlessJump: %s,\n"
			"	isHammerHitDisabled: %s,\n"
			"	isGrenadeHitDisabled: %s,\n"
			"	isLaserHitDisabled: %s,\n"
			"	isShotgunHitDisabled: %s,\n"
			"	isHookHitDisabled: %s,\n"
			"	isSuper: %s,\n"
			"	hasTelegunGun: %s,\n"
			"	hasTelegunGrenade: %s,\n"
			"	hasTelegunLaser: %s,\n"
			"	freezeStart: %d,\n"
			"	freezeEnd: %d,\n"
			"	isInFreeze: %s,\n"
			"	isDeepFrozen: %s,\n"
			"	isLiveFrozen: %s\n"
		")",
		pos_str, vel_str, hookPos_str, hookDir_str, hookTeleBase_str, self->hookTick, self->hookState,
		self->hookedPlayer, self->activeWeapon, self->isNewHook ? "true" : "false", self->jumped, self->jumpedTotal, self->jumps,
		self->direction, self->angle, self->triggeredEvents, self->id, self->isReset ? "true" : "false", lastVel_str,
		self->colliding, self->isLeftWall ? "true" : "false", self->isSolo ? "true" : "false", self->isJetpack ? "true" : "false",
		self->isCollisionDisabled ? "true" : "false", self->isEndlessHook ? "true" : "false", self->isEndlessJump ? "true" : "false",
		self->isHammerHitDisabled ? "true" : "false", self->isGrenadeHitDisabled ? "true" : "false",
		self->isLaserHitDisabled ? "true" : "false", self->isShotgunHitDisabled ? "true" : "false",
		self->isHookHitDisabled ? "true" : "false", self->isSuper ? "true" : "false", self->hasTelegunGun ? "true" : "false",
		self->hasTelegunGrenade ? "true" : "false", self->hasTelegunLaser ? "true" : "false", self->freezeStart,
		self->freezeEnd, self->isInFreeze ? "true" : "false", self->isDeepFrozen ? "true" : "false", self->isLiveFrozen ? "true" : "false"
	);

	// Уменьшение счетчика ссылок на все PyObject.
	Py_DECREF(pos_str_obj);
	Py_DECREF(vel_str_obj);
	Py_DECREF(hookPos_str_obj);
	Py_DECREF(hookDir_str_obj);
	Py_DECREF(hookTeleBase_str_obj);
	Py_DECREF(lastVel_str_obj);

	return PyUnicode_FromString(buf);
}

static PyTypeObject TeeType = {
	{ PyObject_HEAD_INIT(NULL) 0, },
	"API.Tee",                /* tp_name */
	sizeof(Tee),              /* tp_basicsize */
	0,                            /* tp_itemsize */
	(destructor)Tee_dealloc,  /* tp_dealloc */
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
	(reprfunc)Tee_str,                            /* tp_str */
	0,                            /* tp_getattro */
	0,                            /* tp_setattro */
	0,                            /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,           /* tp_flags */
	"Tee",           /* tp_doc */
	0,                            /* tp_traverse */
	0,                            /* tp_clear */
	0,                            /* tp_richcompare */
	0,                            /* tp_weaklistoffset */
	0,                            /* tp_iter */
	0,                            /* tp_iternext */
	Tee_methods,              /* tp_methods */
	0,                            /* tp_members */
	Tee_getseters,                            /* tp_getset */
	0,                            /* tp_base */
	0,                            /* tp_dict */
	0,                            /* tp_descr_get */
	0,                            /* tp_descr_set */
	0,                            /* tp_dictoffset */
	(initproc)Tee_init,      /* tp_init */
	0,                            /* tp_alloc */
	PyType_GenericNew,            /* tp_new */
};

#endif // DDNET_API_TEE_H
