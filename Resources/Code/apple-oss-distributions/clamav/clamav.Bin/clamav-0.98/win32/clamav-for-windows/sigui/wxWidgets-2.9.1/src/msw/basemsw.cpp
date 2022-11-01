///////////////////////////////////////////////////////////////////////////////
// Name:        msw/basemsw.cpp
// Purpose:     misc stuff only used in console applications under MSW
// Author:      Vadim Zeitlin
// Modified by:
// Created:     22.06.2003
// RCS-ID:      $Id$
// Copyright:   (c) 2003 Vadim Zeitlin <vadim@wxwindows.org>
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// ============================================================================
// declarations
// ============================================================================

// ----------------------------------------------------------------------------
// headers
// ----------------------------------------------------------------------------

// for compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#ifndef WX_PRECOMP
    #include "wx/event.h"
#endif //WX_PRECOMP

#include "wx/apptrait.h"
#include "wx/evtloop.h"
#include "wx/msw/private/timer.h"
// MBN: this is a workaround for MSVC 5: if it is not #included in
// some wxBase file, wxRecursionGuard methods won't be exported from
// wxBase.dll, and MSVC 5 will give linker errors
#include "wx/recguard.h"

#include "wx/crt.h"
#include "wx/msw/private.h"

// ============================================================================
// wxAppTraits implementation
// ============================================================================

WXDWORD wxAppTraits::DoSimpleWaitForThread(WXHANDLE hThread)
{
    return ::WaitForSingleObject((HANDLE)hThread, INFINITE);
}

// ============================================================================
// wxConsoleAppTraits implementation
// ============================================================================

void *wxConsoleAppTraits::BeforeChildWaitLoop()
{
    // nothing to do here
    return NULL;
}

void wxConsoleAppTraits::AfterChildWaitLoop(void * WXUNUSED(data))
{
    // nothing to do here
}

bool wxConsoleAppTraits::DoMessageFromThreadWait()
{
    // nothing to process here
    return true;
}

#if wxUSE_TIMER

wxTimerImpl *wxConsoleAppTraits::CreateTimerImpl(wxTimer *timer)
{
    return new wxMSWTimerImpl(timer);
}

#endif // wxUSE_TIMER

wxEventLoopBase *wxConsoleAppTraits::CreateEventLoop()
{
#if wxUSE_CONSOLE_EVENTLOOP
    return new wxEventLoop();
#else // !wxUSE_CONSOLE_EVENTLOOP
    return NULL;
#endif // wxUSE_CONSOLE_EVENTLOOP/!wxUSE_CONSOLE_EVENTLOOP
}


WXDWORD wxConsoleAppTraits::WaitForThread(WXHANDLE hThread)
{
    return DoSimpleWaitForThread(hThread);
}

bool wxConsoleAppTraits::WriteToStderr(const wxString& text)
{
    return wxFprintf(stderr, "%s", text) != -1;
}
