
static const void* 
mod_observer_retain(const void* info) 
{
	PyGILState_STATE state = PyGILState_Ensure();
	Py_INCREF((PyObject*)info);
	PyGILState_Release(state);
	return info;
}

static void
mod_observer_release(const void* info)
{
	PyGILState_STATE state = PyGILState_Ensure();
	Py_DECREF((PyObject*)info);
	PyGILState_Release(state);
}


static CFRunLoopObserverContext mod_CFRunLoopObserverContext = {
	0,		
	NULL,
	mod_observer_retain,
	mod_observer_release,
	NULL
};

static void
mod_CFRunLoopObserverCallBack(	
	CFRunLoopObserverRef f,
	CFRunLoopActivity activity,
	void* _info)
{
	PyObject* info = (PyObject*)_info;
	PyGILState_STATE state = PyGILState_Ensure();

	PyObject* py_f = PyObjC_ObjCToPython(@encode(CFRunLoopObserverRef), &f);
	PyObject* py_activity = PyObjC_ObjCToPython(
		@encode(CFRunLoopActivity), &activity);

	PyObject* result = PyObject_CallFunction(
		PyTuple_GetItem(info, 0),
		"NNO", py_f, py_activity, PyTuple_GetItem(info, 1));
	if (result == NULL) {
		PyObjCErr_ToObjCWithGILState(&state);
	}
	Py_DECREF(result);
	PyGILState_Release(state);
}

static PyObject*
mod_CFRunLoopObserverCreate(
	PyObject* self __attribute__((__unused__)),
	PyObject* args)
{
	PyObject* py_allocator;
	PyObject* py_activities;
	PyObject* py_repeats;
	PyObject* py_order;
	PyObject* callout;
	PyObject* info;
	CFAllocatorRef allocator;
	CFOptionFlags activities;
	Boolean repeats;
	CFIndex order;

	if (!PyArg_ParseTuple(args, "OOOOOO", &py_allocator, &py_activities, &py_repeats, &py_order, &callout, &info)) {
		return NULL;
	}

	if (PyObjC_PythonToObjC(@encode(CFAllocatorRef), py_allocator, &allocator) < 0) {
		return NULL;
	}
	if (PyObjC_PythonToObjC(@encode(CFOptionFlags), py_activities, &activities) < 0) {
		return NULL;
	}
	if (PyObjC_PythonToObjC(@encode(bool), py_repeats, &repeats) < 0) {
		return NULL;
	}
	if (PyObjC_PythonToObjC(@encode(CFIndex), py_order, &order) < 0) {
		return NULL;
	}

	CFRunLoopObserverContext context = mod_CFRunLoopObserverContext;
	context.info = Py_BuildValue("OO", callout, info);
	if (context.info == NULL) {
		return NULL;
	}

	CFRunLoopObserverRef rv = NULL;
	PyObjC_DURING
		rv = CFRunLoopObserverCreate(
			allocator, activities, repeats, order,
			mod_CFRunLoopObserverCallBack, &context);
		

	PyObjC_HANDLER
		rv = NULL;
		PyObjCErr_FromObjC(localException);

	PyObjC_ENDHANDLER

	Py_DECREF((PyObject*)context.info);
	if (PyErr_Occurred()) {
		return NULL;
	}

	PyObject* result =  PyObjC_ObjCToPython(@encode(CFRunLoopObserverRef), &rv);
	if (rv != NULL) {
		CFRelease(rv);
	}
	return result;
}


static PyObject*
mod_CFRunLoopObserverGetContext(
	PyObject* self __attribute__((__unused__)),
	PyObject* args)
{
	PyObject* py_f;
	PyObject* py_context;
	CFRunLoopObserverRef f;
	CFRunLoopObserverContext context;

	if (!PyArg_ParseTuple(args, "OO", &py_f, &py_context)) {
		return NULL;
	}

	if (py_context != NULL &&  py_context != Py_None) {
		PyErr_SetString(PyExc_ValueError, "invalid context");
		return NULL;
	}

	if (PyObjC_PythonToObjC(@encode(CFRunLoopObserverRef), py_f, &f) < 0) {
		return NULL;
	}

	context.version = 0;

	PyObjC_DURING
		CFRunLoopObserverGetContext(f, &context);

	PyObjC_HANDLER
		PyObjCErr_FromObjC(localException);

	PyObjC_ENDHANDLER

	if (PyErr_Occurred()) {
		return NULL;
	}

	if (context.version != 0) {
		PyErr_SetString(PyExc_ValueError, "retrieved context is not valid");
		return NULL;
	}

	if (context.retain != mod_observer_retain) {
		PyErr_SetString(PyExc_ValueError, 
			"retrieved context is not supported");
		return NULL;
	}

	if (context.info == NULL) {
		Py_INCREF(PyObjC_NULL);
		return PyObjC_NULL;
	}


	Py_INCREF(PyTuple_GetItem((PyObject*)context.info, 1));
	return PyTuple_GetItem((PyObject*)context.info, 1);
}

#define COREFOUNDATION_RUNLOOP_METHODS \
        {	\
		"CFRunLoopObserverCreate",	\
		(PyCFunction)mod_CFRunLoopObserverCreate,	\
		METH_VARARGS,	\
		NULL	\
	},	\
        {	\
		"CFRunLoopObserverGetContext",	\
		(PyCFunction)mod_CFRunLoopObserverGetContext,	\
		METH_VARARGS,	\
		NULL	\
	},
