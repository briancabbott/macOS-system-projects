// natConstructor.cc - Native code for Constructor class.

/* Copyright (C) 1999, 2000, 2001, 2002, 2003  Free Software Foundation

   This file is part of libgcj.

This software is copyrighted work licensed under the terms of the
Libgcj License.  Please consult the file "LIBGCJ_LICENSE" for
details.  */

#include <config.h>

#include <gcj/cni.h>
#include <jvm.h>

#include <java/lang/ArrayIndexOutOfBoundsException.h>
#include <java/lang/IllegalAccessException.h>
#include <java/lang/reflect/Constructor.h>
#include <java/lang/reflect/Method.h>
#include <java/lang/reflect/InvocationTargetException.h>
#include <java/lang/reflect/Modifier.h>
#include <java/lang/InstantiationException.h>
#include <gcj/method.h>

jint
java::lang::reflect::Constructor::getModifiers ()
{
  // Ignore all unknown flags.
  return _Jv_FromReflectedConstructor (this)->accflags & Modifier::ALL_FLAGS;
}

void
java::lang::reflect::Constructor::getType ()
{
  _Jv_GetTypesFromSignature (_Jv_FromReflectedConstructor (this),
			     declaringClass,
			     &parameter_types,
			     NULL);

  // FIXME: for now we have no way to get exception information.
  exception_types = 
    (JArray<jclass> *) JvNewObjectArray (0, &java::lang::Class::class$, NULL);
}

jobject
java::lang::reflect::Constructor::newInstance (jobjectArray args)
{
  using namespace java::lang::reflect;

  if (parameter_types == NULL)
    getType ();

  jmethodID meth = _Jv_FromReflectedConstructor (this);

  // Check accessibility, if required.
  if (! (Modifier::isPublic (meth->accflags) || this->isAccessible()))
    {
      gnu::gcj::runtime::StackTrace *t 
	= new gnu::gcj::runtime::StackTrace(4);
      Class *caller = NULL;
      try
	{
	  for (int i = 1; !caller; i++)
	    {
	      caller = t->classAt (i);
	    }
	}
      catch (::java::lang::ArrayIndexOutOfBoundsException *e)
	{
	}

      if (! _Jv_CheckAccess(caller, declaringClass, meth->accflags))
	throw new IllegalAccessException;
    }

  if (Modifier::isAbstract (declaringClass->getModifiers()))
    throw new InstantiationException;

  _Jv_InitClass (declaringClass);

  // In the constructor case the return type is the type of the
  // constructor.
  return _Jv_CallAnyMethodA (NULL, declaringClass, meth, true,
			     parameter_types, args);
}
