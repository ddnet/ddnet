#ifndef DDNET_API_VECTOR2_H
#define DDNET_API_VECTOR2_H

#include "Python.h"
#include "game/client/gameclient.h"

typedef struct {
	PyObject_HEAD;
	double x;
	double y;
} Vector2;

extern PyTypeObject Vector2Type;

static PyObject *Vector2_distance(Vector2* self, PyObject *args)
{
	Vector2 *other_vec;

	if (!PyArg_ParseTuple(args, "O!", &Vector2Type, &other_vec))
		return NULL;

	double distance = sqrt(pow(self->x - other_vec->x, 2) + pow(self->y - other_vec->y, 2));

	return PyFloat_FromDouble(distance);
}

static PyMethodDef Vector2_methods[] = {
	{"distance", (PyCFunction)Vector2_distance, METH_VARARGS, "distance(vector: Vector2)\nCalculate distance between two vectors"},
	{NULL}  /* Sentinel */
};

static int Vector2_init(Vector2 *self, PyObject *args, PyObject *kwds)
{
	if (!PyArg_ParseTuple(args, "dd", &self->x, &self->y))
		return -1;

	return 0;
}

// Высвобождение ресурсов
static void Vector2_dealloc(Vector2* self)
{
	Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Vector2_getX(Vector2 *self, void *closure)
{
	return PyFloat_FromDouble(self->x);
}

static PyObject* Vector2_getY(Vector2 *self, void *closure)
{
	return PyFloat_FromDouble(self->y);
}

static int Vector2_setX(Vector2 *self, PyObject *value, void *closure)
{
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete the attribute");
		return -1;
	}

	if (!PyFloat_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "The attribute value must be a float");
		return -1;
	}

	self->x = PyFloat_AsDouble(value);
	return 0;
}

static int Vector2_setY(Vector2 *self, PyObject *value, void *closure)
{
	if (value == NULL) {
		PyErr_SetString(PyExc_TypeError, "Cannot delete the attribute");
		return -1;
	}

	if (!PyFloat_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "The attribute value must be a float");
		return -1;
	}

	self->y = PyFloat_AsDouble(value);
	return 0;
}

static PyGetSetDef Vector2_getseters[] = {
	{"x", (getter) Vector2_getX, (setter) Vector2_setX, "x", NULL},
	{"y", (getter) Vector2_getY, (setter) Vector2_setY, "y", NULL},
	{NULL}  /* Sentinel */
};

static PyObject* Vector2_add(PyObject* v, PyObject* w) {
	if (!PyObject_TypeCheck(w, &Vector2Type)) {
		PyErr_SetString(PyExc_TypeError, "The second operand must be a Vector2");
		return NULL;
	}

	Vector2 *vVector = (Vector2 *)v;
	Vector2 *wVector = (Vector2 *)w;

	Vector2 *result = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	result->x = vVector->x + wVector->x;
	result->y = vVector->y + wVector->y;

	return (PyObject *)result;
}

static PyObject* Vector2_sub(PyObject* v, PyObject* w) {
	if (!PyObject_TypeCheck(w, &Vector2Type)) {
		PyErr_SetString(PyExc_TypeError, "The second operand must be a Vector2");
		return NULL;
	}

	Vector2 *vVector = (Vector2 *)v;
	Vector2 *wVector = (Vector2 *)w;

	Vector2 *result = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	result->x = vVector->x - wVector->x;
	result->y = vVector->y - wVector->y;

	return (PyObject *)result;
}

static PyObject* Vector2_mul(PyObject* v, PyObject* w)
{
	double xMul, yMul;

	if (PyFloat_Check(w)) {
		xMul = PyFloat_AS_DOUBLE(w);
		yMul = xMul;
	} else if (PyLong_Check(w)) {
		xMul = (double)PyLong_AsLong(w);
		yMul = xMul;
	} else if (PyObject_TypeCheck(w, &Vector2Type)) {
		Vector2 *wVector = (Vector2 *)w;
		xMul = wVector->x;
		yMul = wVector->y;
	} else {
		PyErr_SetString(PyExc_TypeError, "The operand must be a int, float or Vector2");
		return NULL;
	}

	Vector2 *vec = (Vector2 *)v;

	Vector2 *result = PyObject_New(Vector2, &Vector2Type);
	result->x = vec->x * xMul;
	result->y = vec->y * yMul;

	return (PyObject *)result;
}

static PyObject* Vector2_div(PyObject* v, PyObject* w) {

	double xDiv, yDiv;

	if (PyFloat_Check(w)) {
		xDiv = PyFloat_AS_DOUBLE(w);
		yDiv = xDiv;
	} else if (PyLong_Check(w)) {
		xDiv = (double)PyLong_AsLong(w);
		yDiv = xDiv;
	} else if (PyObject_TypeCheck(w, &Vector2Type)) {
		Vector2 *wVector = (Vector2 *)w;
		xDiv = wVector->x;
		yDiv = wVector->y;
	} else {
		PyErr_SetString(PyExc_TypeError, "The operand must be a int, float or Vector2");
		return NULL;
	}

	if (xDiv == 0 || yDiv == 0) {
		PyErr_SetString(PyExc_ZeroDivisionError, "Division by zero");
		return NULL;
	}

	Vector2 *vVector = (Vector2 *)v;

	Vector2 *result = (Vector2 *)PyObject_New(Vector2, &Vector2Type);
	result->x = vVector->x / xDiv;
	result->y = vVector->y / yDiv;

	return (PyObject *)result;
}

// Определение методов числового протокола
static PyNumberMethods Vector2_as_number = {
	Vector2_add,
	Vector2_sub,
	Vector2_mul,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	Vector2_div,
};

static PyObject* Vector2_str(Vector2* self)
{
	char buf[19];
	sprintf(buf, "{x: %.2f, y: %.2f}", self->x, self->y);
	return PyUnicode_FromString(buf);
}

static PyTypeObject Vector2Type = {
	{ PyObject_HEAD_INIT(NULL) 0, },
	"API.Vector2",                /* tp_name */
	sizeof(Vector2),              /* tp_basicsize */
	0,                            /* tp_itemsize */
	(destructor)Vector2_dealloc,  /* tp_dealloc */
	0,                            /* tp_print */
	0,                            /* tp_getattr */
	0,                            /* tp_setattr */
	0,                            /* tp_compare */
	0,                            /* tp_repr */
	&Vector2_as_number,               /* tp_as_number */
	0,                            /* tp_as_sequence */
	0,                            /* tp_as_mapping */
	0,                            /* tp_hash */
	0,                            /* tp_call */
	(reprfunc)Vector2_str,                            /* tp_str */
	0,                            /* tp_getattro */
	0,                            /* tp_setattro */
	0,                            /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,           /* tp_flags */
	"Vector2",           /* tp_doc */
	0,                            /* tp_traverse */
	0,                            /* tp_clear */
	0,                            /* tp_richcompare */
	0,                            /* tp_weaklistoffset */
	0,                            /* tp_iter */
	0,                            /* tp_iternext */
	Vector2_methods,              /* tp_methods */
	0,                            /* tp_members */
	Vector2_getseters,                            /* tp_getset */
	0,                            /* tp_base */
	0,                            /* tp_dict */
	0,                            /* tp_descr_get */
	0,                            /* tp_descr_set */
	0,                            /* tp_dictoffset */
	(initproc)Vector2_init,      /* tp_init */
	0,                            /* tp_alloc */
	PyType_GenericNew,            /* tp_new */
};

#endif // DDNET_API_VECTOR2_H
