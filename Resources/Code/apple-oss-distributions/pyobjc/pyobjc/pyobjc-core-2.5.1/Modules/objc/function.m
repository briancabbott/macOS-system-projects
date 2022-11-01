/*
 * Wrapper for simple global functions. Simple functions are those without
 * arguments that require additional effort.
 */
#include "pyobjc.h"

/* XXX: move this to a header file */
static inline Py_ssize_t align(Py_ssize_t offset, Py_ssize_t alignment)
{
	Py_ssize_t rest = offset % alignment;
	if (rest == 0) return offset;
	return offset + (alignment - rest);
}


typedef struct {
	PyObject_HEAD
	ffi_cif*  cif;
	PyObjCMethodSignature* methinfo;
	void*     function;
	PyObject* doc;
	PyObject* name;
	PyObject* module;
} func_object;



PyDoc_STRVAR(func_metadata_doc, "Return a dict that describes the metadata for this function.");
static PyObject* func_metadata(PyObject* self)
{
	return PyObjCMethodSignature_AsDict(((func_object*)self)->methinfo);
}

static PyMethodDef func_methods[] = {
	{
		  "__metadata__",
	    	  (PyCFunction)func_metadata,
	          METH_NOARGS,
		  func_metadata_doc
	},
	{ 0, 0, 0, 0 }
};



static PyMemberDef func_members[] = {
	{
		"__doc__",
		T_OBJECT,
		offsetof(func_object, doc),
		READONLY,
		NULL
	},
	{
		"__name__",
		T_OBJECT,
		offsetof(func_object, name),
		READONLY,
		NULL
	},
	{
		"__module__",
		T_OBJECT,
		offsetof(func_object, module),
		0,
		NULL
	},
	{
		NULL,
		0,
		0,
		0,
		NULL
	}
};

static PyObject* func_repr(PyObject* _self)
{
	func_object* self = (func_object*)_self;

	if (self->name == NULL) {
		return PyText_FromFormat("<objc.function object at %p>", self);
#if PY_MAJOR_VERSION == 2
	} else if (PyString_Check(self->name)) {
		return PyString_FromFormat("<objc.function '%s' at %p>", PyString_AsString(self->name), self);
#else
	} else if (PyUnicode_Check(self->name)) {
		return PyUnicode_FromFormat("<objc.function %R at %p>", self->name, self);
#endif
	} else {
#if PY_MAJOR_VERSION == 2
		PyObject* result;
		PyObject* name_repr = PyObject_Repr(self->name);
		if (name_repr == NULL) {
			return NULL;
		}
		if (!PyString_Check(name_repr)) {
			result = PyString_FromFormat("<objc.function object at %p>", self);
		} else {
			result = PyString_FromFormat("<objc.function '%s' at %p>", 
					PyString_AsString(name_repr), self);
		}
		Py_DECREF(name_repr);
		return result;
#else
		return PyUnicode_FromFormat("<objc.function %R at %p>", self->name, self);
#endif
	}
}


static PyObject* 
func_call(PyObject* s, PyObject* args, PyObject* kwds)
{
	func_object* self = (func_object*)s;
	Py_ssize_t byref_in_count;
	Py_ssize_t byref_out_count;
	Py_ssize_t plain_count;
	Py_ssize_t argbuf_len;
	int r;
	int cif_arg_count;
	BOOL variadicAllArgs = NO;

	unsigned char* argbuf = NULL;
	ffi_type* arglist[64];
	void*     values[64];
	void**	  byref = NULL;
	struct byref_attr* byref_attr = NULL;
	ffi_cif cif;
	ffi_cif* volatile cifptr;

	PyObject* retval;	

	if (self->methinfo->suggestion != NULL) {
		PyErr_SetObject(PyExc_TypeError, self->methinfo->suggestion);
		return NULL;
	}


	if (Py_SIZE(self->methinfo) >= 63) {
		PyErr_Format(PyObjCExc_Error,
			"wrapping a function with %"PY_FORMAT_SIZE_T"d arguments, at most 64 "
			"are supported", Py_SIZE(self->methinfo));
		return NULL;
	}

	if (kwds != NULL && (!PyDict_Check(kwds) || PyDict_Size(kwds) != 0)) {
		PyErr_SetString(PyExc_TypeError,
			"keyword arguments not supported");
		return NULL;
	}

	argbuf_len = PyObjCRT_SizeOfReturnType(self->methinfo->rettype.type);
	argbuf_len = align(argbuf_len, sizeof(void*));
	r = PyObjCFFI_CountArguments(
		self->methinfo, 0,
		&byref_in_count, &byref_out_count, &plain_count,
		&argbuf_len, &variadicAllArgs);
	if (r == -1) {
		return NULL;
	}

	variadicAllArgs |= self->methinfo->variadic && (self->methinfo->null_terminated_array || self->methinfo->arrayArg != -1);

	if (variadicAllArgs) {
		if (byref_in_count != 0 || byref_out_count != 0) {
			PyErr_Format(PyExc_TypeError, "Sorry, printf format with by-ref args not supported");
			return NULL;
		}
		if (PyTuple_Size(args) < Py_SIZE(self->methinfo)) {
			PyErr_Format(PyExc_TypeError, "Need %"PY_FORMAT_SIZE_T"d arguments, got %"PY_FORMAT_SIZE_T"d",
			Py_SIZE(self->methinfo) - 2, PyTuple_Size(args));
			return NULL;
		}
	} else if (PyTuple_Size(args) != Py_SIZE(self->methinfo)) {
		PyErr_Format(PyExc_TypeError, "Need %"PY_FORMAT_SIZE_T"d arguments, got %"PY_FORMAT_SIZE_T"d",
			Py_SIZE(self->methinfo), PyTuple_Size(args));
		return NULL;
	}


	argbuf = PyMem_Malloc(argbuf_len);
	if (argbuf == NULL) {
		PyErr_NoMemory();
		return NULL;
	}

	if (variadicAllArgs) {
		if (PyObjCFFI_AllocByRef(Py_SIZE(self->methinfo)+PyTuple_Size(args), &byref, &byref_attr) < 0) {
			goto error;
		}
	} else {
		if (PyObjCFFI_AllocByRef(Py_SIZE(self->methinfo), &byref, &byref_attr) < 0) {
			goto error;
		}
	}

	cif_arg_count = PyObjCFFI_ParseArguments(
		self->methinfo, 0, args,
		align(PyObjCRT_SizeOfReturnType(self->methinfo->rettype.type), sizeof(void*)),
		argbuf, argbuf_len, byref, byref_attr, arglist, values);
	if (cif_arg_count == -1) {
		goto error;
	}

	if (variadicAllArgs) {
		r = ffi_prep_cif_var(&cif, FFI_DEFAULT_ABI, Py_SIZE(self->methinfo), cif_arg_count,
			signature_to_ffi_return_type(self->methinfo->rettype.type), arglist);
		if (r != FFI_OK) {
			PyErr_Format(PyExc_RuntimeError,
				"Cannot setup FFI CIF [%d]", r);
				goto error;
		}
		cifptr = &cif;

	} else {
		cifptr = self->cif;
	}

	PyObjC_DURING
		ffi_call(cifptr, FFI_FN(self->function), argbuf, values);
	PyObjC_HANDLER
		PyObjCErr_FromObjC(localException);
	PyObjC_ENDHANDLER

	if (PyErr_Occurred()) {
		goto error;
	}

	retval = PyObjCFFI_BuildResult(self->methinfo, 0, argbuf, byref, 
			byref_attr, byref_out_count, NULL, 0, values);

	if (variadicAllArgs) {
		if (PyObjCFFI_FreeByRef(Py_SIZE(self->methinfo)+PyTuple_Size(args), byref, byref_attr) < 0) {
			byref = NULL; byref_attr = NULL;
			goto error;
		}
	} else {
		if (PyObjCFFI_FreeByRef(Py_SIZE(self->methinfo), byref, byref_attr) < 0) {
			byref = NULL; byref_attr = NULL;
			goto error;
		}
	}
	PyMem_Free(argbuf); argbuf = NULL;
	return retval;

error:
	if (variadicAllArgs) {
		if (PyObjCFFI_FreeByRef(PyTuple_Size(args), byref, byref_attr) < 0) {
			byref = NULL; byref_attr = NULL;
			goto error;
		}
	} else {
		if (PyObjCFFI_FreeByRef(Py_SIZE(self->methinfo), byref, byref_attr) < 0) {
			byref = NULL; byref_attr = NULL;
			goto error;
		}
	}
	if (argbuf) {
		PyMem_Free(argbuf);
	}
	return NULL;
}

static void 
func_dealloc(PyObject* s)
{
	func_object* self = (func_object*)s;

	Py_XDECREF(self->doc); self->doc = NULL;
	Py_XDECREF(self->name); self->name = NULL;
	Py_XDECREF(self->module); self->module = NULL;
	if (self->cif != NULL) {
		PyObjCFFI_FreeCIF(self->cif);
	}
	Py_XDECREF(self->methinfo);
	PyObject_Free(s);
}

PyTypeObject PyObjCFunc_Type =
{
	PyVarObject_HEAD_INIT(&PyType_Type, 0)
	"objc.function",			/* tp_name */
	sizeof (func_object),			/* tp_basicsize */
	0,					/* tp_itemsize */
  
	/* methods */
	func_dealloc,				/* tp_dealloc */
	0,					/* tp_print */
	0,					/* tp_getattr */
	0,					/* tp_setattr */
	0,					/* tp_compare */
	func_repr,				/* tp_repr */
	0,					/* tp_as_number */
	0,					/* tp_as_sequence */
	0,					/* tp_as_mapping */
	0,					/* tp_hash */
	func_call,				/* tp_call */
	0,					/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
	0,					/* tp_setattro */
	0,					/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	"Wrapper around a Objective-C function",/* tp_doc */
	0,					/* tp_traverse */
	0,					/* tp_clear */
	0,					/* tp_richcompare */
	0,					/* tp_weaklistoffset */
	0,					/* tp_iter */
	0,					/* tp_iternext */
	func_methods,				/* tp_methods */
	func_members,				/* tp_members */
	0,					/* tp_getset */
	0,					/* tp_base */
	0,					/* tp_dict */
	0,					/* tp_descr_get */
	0,					/* tp_descr_set */
	0,					/* tp_dictoffset */
	0,					/* tp_init */
	0,					/* tp_alloc */
	0,					/* tp_new */
	0,					/* tp_free */
	0,					/* tp_is_gc */
	0,					/* tp_bases */
	0,					/* tp_mro */
	0,					/* tp_cache */
	0,					/* tp_subclasses */
	0,					/* tp_weaklist */
	0					/* tp_del */
#if PY_VERSION_HEX >= 0x02060000
	, 0                                     /* tp_version_tag */
#endif

};

PyObject*
PyObjCFunc_WithMethodSignature(PyObject* name, void* func, PyObjCMethodSignature* methinfo)
{
	func_object* result;

	result = PyObject_NEW(func_object, &PyObjCFunc_Type);
	if (result == NULL) return NULL;

	result->function = func;
	result->doc = NULL;
	result->name = name;
	Py_XINCREF(name);
	result->module = NULL;
	result->methinfo = methinfo;
	Py_XINCREF(methinfo);

	result->cif = PyObjCFFI_CIFForSignature(result->methinfo);
	if (result->cif == NULL) {
		Py_DECREF(result);
		return NULL;	
	}
	
	return (PyObject*)result;
}


PyObject* 
PyObjCFunc_New(PyObject* name, void* func, const char* signature, PyObject* doc, PyObject* meta)
{
	func_object* result;
	

	result = PyObject_NEW(func_object, &PyObjCFunc_Type);
	if (result == NULL) return NULL;

	result->function = NULL;
	result->doc = NULL;
	result->name = NULL;
	result->module = NULL;
	result->cif = NULL;

	result->methinfo= PyObjCMethodSignature_WithMetaData(signature, meta, NO);
	if (result->methinfo == NULL) {
		Py_DECREF(result);
		return NULL;
	}

	result->function = func;

	result->doc = doc;
	Py_XINCREF(doc);

	result->name = name;
	Py_XINCREF(name);
	result->cif = PyObjCFFI_CIFForSignature(result->methinfo);
	if (result->cif == NULL) {
		Py_DECREF(result);
		return NULL;	
	}
	
	return (PyObject*)result;
}

PyObjCMethodSignature* PyObjCFunc_GetMethodSignature(PyObject* func)
{
	return ((func_object*)func)->methinfo;
}
