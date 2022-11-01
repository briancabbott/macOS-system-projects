#if MAC_OS_X_VERSION_10_5 <= MAC_OS_X_VERSION_MAX_ALLOWED

/* Needed when building against the OS X 10.5+ SDK but running on 10.4. */
#pragma weak CFFileDescriptorCreate
#pragma weak CFFileDescriptorGetContext

static void* 
mod_filedescr_retain(void* info) 
{
	PyGILState_STATE state = PyGILState_Ensure();
	Py_INCREF((PyObject*)info);
	PyGILState_Release(state);
	return info;
}

static void
mod_filedescr_release(void* info)
{
	PyGILState_STATE state = PyGILState_Ensure();
	Py_DECREF((PyObject*)info);
	PyGILState_Release(state);
}


static CFFileDescriptorContext mod_CFFileDescriptorContext = {
	0,		
	NULL,
	mod_filedescr_retain,
	mod_filedescr_release,
	NULL
};

static void
mod_CFFileDescriptorCallBack(	
	CFFileDescriptorRef f,
	CFOptionFlags callBackType,
	void* _info)
{
	PyObject* info = (PyObject*)_info;
	PyGILState_STATE state = PyGILState_Ensure();

	PyObject* py_f = PyObjC_ObjCToPython(@encode(CFFileDescriptorRef), &f);
	PyObject* py_callBackType = PyObjC_ObjCToPython(
		@encode(CFOptionFlags), &callBackType);

	PyObject* result = PyObject_CallFunction(
		PyTuple_GetItem(info, 0),
		"NNO", py_f, py_callBackType, PyTuple_GetItem(info, 1));
	if (result == NULL) {
		PyObjCErr_ToObjCWithGILState(&state);
	}
	Py_DECREF(result);
	PyGILState_Release(state);
}

static PyObject*
mod_CFFileDescriptorCreate(
	PyObject* self __attribute__((__unused__)),
	PyObject* args)
{
	PyObject* py_allocator;
	PyObject* py_descriptor;
	PyObject* py_closeOnInvalidate;
	PyObject* callout;
	PyObject* info;
	CFAllocatorRef allocator;
	CFFileDescriptorNativeDescriptor descriptor;
	Boolean closeOnInvalidate;

	if (!PyArg_ParseTuple(args, "OOOOO", &py_allocator, &py_descriptor, &py_closeOnInvalidate, &callout, &info)) {
		return NULL;
	}

	if (PyObjC_PythonToObjC(@encode(CFAllocatorRef), py_allocator, &allocator) < 0) {
		return NULL;
	}
	if (PyObjC_PythonToObjC(@encode(CFFileDescriptorNativeDescriptor), py_descriptor, &descriptor) < 0) {
		return NULL;
	}
	if (PyObjC_PythonToObjC(@encode(bool), py_closeOnInvalidate, &closeOnInvalidate) < 0) {
		return NULL;
	}

	CFFileDescriptorContext context = mod_CFFileDescriptorContext;
	context.info = Py_BuildValue("OO", callout, info);
	if (context.info == NULL) {
		return NULL;
	}

	CFFileDescriptorRef rv = NULL;
	PyObjC_DURING
		rv = CFFileDescriptorCreate(
			allocator, descriptor, closeOnInvalidate,
			mod_CFFileDescriptorCallBack, &context);
		

	PyObjC_HANDLER
		rv = NULL;
		PyObjCErr_FromObjC(localException);

	PyObjC_ENDHANDLER

	Py_DECREF((PyObject*)context.info);
	if (PyErr_Occurred()) {
		return NULL;
	}

	PyObject* result =  PyObjC_ObjCToPython(@encode(CFFileDescriptorRef), &rv);
	if (rv != NULL) {
		CFRelease(rv);
	}
	return result;
}


static PyObject*
mod_CFFileDescriptorGetContext(
	PyObject* self __attribute__((__unused__)),
	PyObject* args)
{
	PyObject* py_f;
	PyObject* py_context;
	CFFileDescriptorRef f;
	CFFileDescriptorContext context;

	if (!PyArg_ParseTuple(args, "OO", &py_f, &py_context)) {
		return NULL;
	}

	if (py_context != Py_None) {
		PyErr_SetString(PyExc_ValueError, "invalid context");
		return NULL;
	}

	if (PyObjC_PythonToObjC(@encode(CFFileDescriptorRef), py_f, &f) < 0) {
		return NULL;
	}

	context.version = 0;

	PyObjC_DURING
		CFFileDescriptorGetContext(f, &context);

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

	if (context.retain != mod_filedescr_retain) {
		PyErr_SetString(PyExc_ValueError, 
			"retrieved context is not supported");
		return NULL;
	}

	Py_INCREF(PyTuple_GetItem((PyObject*)context.info, 1));
	return PyTuple_GetItem((PyObject*)context.info, 1);
}
#endif

#define COREFOUNDATION_FILEDESCRIPTOR_METHODS		\
        {		\
		"CFFileDescriptorCreate",		\
		(PyCFunction)mod_CFFileDescriptorCreate,		\
		METH_VARARGS,		\
		NULL		\
	},		\
        {		\
		"CFFileDescriptorGetContext",		\
		(PyCFunction)mod_CFFileDescriptorGetContext,		\
		METH_VARARGS,		\
		NULL		\
	},

#define COREFOUNDATION_FILEDESCRIPTOR_AFTER_CREATE		\
	if (&CFFileDescriptorCreate == NULL) {  /* weakly linked */		\
		if (PyObject_DelAttrString(m, "CFFileDescriptorCreate") == -1) {		\
			PyObjC_INITERROR();		\
		}		\
	}		\
	if (&CFFileDescriptorGetContext == NULL) {  /* weakly linked */		\
		if (PyObject_DelAttrString(m, "CFFileDescriptorGetContext") == -1) {		\
			PyObjC_INITERROR();		\
		}		\
	}
