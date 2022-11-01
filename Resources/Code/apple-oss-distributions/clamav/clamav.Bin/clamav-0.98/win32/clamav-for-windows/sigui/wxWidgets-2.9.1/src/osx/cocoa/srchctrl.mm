///////////////////////////////////////////////////////////////////////////////
// Name:        src/osx/carbon/srchctrl.cpp
// Purpose:     implements mac carbon wxSearchCtrl
// Author:      Vince Harron
// Created:     2006-02-19
// RCS-ID:      $Id: srchctrl.cpp 54820 2008-07-29 20:04:11Z SC $
// Copyright:   Vince Harron
// Licence:     wxWindows licence
///////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#ifdef __BORLANDC__
    #pragma hdrstop
#endif

#if wxUSE_SEARCHCTRL

#include "wx/srchctrl.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
#endif //WX_PRECOMP

#if wxUSE_NATIVE_SEARCH_CONTROL

#include "wx/osx/private.h"
#include "wx/osx/cocoa/private/textimpl.h"


@interface wxNSSearchField : NSSearchField
{
}

@end

@implementation wxNSSearchField

+ (void)initialize
{
    static BOOL initialized = NO;
    if (!initialized)
    {
        initialized = YES;
        wxOSXCocoaClassAddWXMethods( self );
    }
}

- (id)initWithFrame:(NSRect)frame
{
    [super initWithFrame:frame];
    [self setTarget: self];
    [self setAction: @selector(searchAction:)];
    return self;
}

- (void) searchAction: (id) sender
{
    (void) sender;
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
    {
        wxSearchCtrl* wxpeer = dynamic_cast<wxSearchCtrl*>( impl->GetWXPeer() );
        if ( wxpeer )
        {
            NSString *searchString = [self stringValue];
            if ( searchString == nil )
            {
                wxpeer->HandleSearchFieldCancelHit();
            }
            else
            {
                wxpeer->HandleSearchFieldSearchHit();
            }
        }
    }
}

- (void)controlTextDidChange:(NSNotification *)aNotification
{
    wxUnusedVar(aNotification);
    wxWidgetCocoaImpl* impl = (wxWidgetCocoaImpl* ) wxWidgetImpl::FindFromWXWidget( self );
    if ( impl )
        impl->controlTextDidChange();
}

@end

// ============================================================================
// wxMacSearchFieldControl
// ============================================================================

class wxNSSearchFieldControl : public wxNSTextFieldControl, public wxSearchWidgetImpl
{
public :
    wxNSSearchFieldControl( wxTextCtrl *wxPeer, wxNSSearchField* w  ) : wxNSTextFieldControl(wxPeer, w)
    {
        m_searchFieldCell = [w cell];
        m_searchField = w;
    }
    ~wxNSSearchFieldControl();

    // search field options
    virtual void ShowSearchButton( bool show )
    {
        if ( show )
            [m_searchFieldCell resetSearchButtonCell];
        else
            [m_searchFieldCell setSearchButtonCell:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual bool IsSearchButtonVisible() const
    {
        return [m_searchFieldCell searchButtonCell] != nil;
    }

    virtual void ShowCancelButton( bool show )
    {
        if ( show )
            [m_searchFieldCell resetCancelButtonCell];
        else
            [m_searchFieldCell setCancelButtonCell:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual bool IsCancelButtonVisible() const
    {
        return [m_searchFieldCell cancelButtonCell] != nil;
    }

    virtual void SetSearchMenu( wxMenu* menu )
    {
        if ( menu )
            [m_searchFieldCell setSearchMenuTemplate:menu->GetHMenu()];
        else
            [m_searchFieldCell setSearchMenuTemplate:nil];
        [m_searchField setNeedsDisplay:YES];
    }

    virtual void SetDescriptiveText(const wxString& text)
    {
        [m_searchFieldCell setPlaceholderString:
            wxCFStringRef( text , m_wxPeer->GetFont().GetEncoding() ).AsNSString()];
    }

    virtual bool SetFocus()
    {
       return  wxNSTextFieldControl::SetFocus();
    }

private:
    wxNSSearchField* m_searchField;
    NSSearchFieldCell* m_searchFieldCell;
} ;

wxNSSearchFieldControl::~wxNSSearchFieldControl()
{
}

wxWidgetImplType* wxWidgetImpl::CreateSearchControl( wxTextCtrl* wxpeer,
                                    wxWindowMac* WXUNUSED(parent),
                                    wxWindowID WXUNUSED(id),
                                    const wxString& str,
                                    const wxPoint& pos,
                                    const wxSize& size,
                                    long WXUNUSED(style),
                                    long WXUNUSED(extraStyle))
{
    NSRect r = wxOSXGetFrameForControl( wxpeer, pos , size ) ;
    wxNSSearchField* v = [[wxNSSearchField alloc] initWithFrame:r];
    [[v cell] setSendsWholeSearchString:YES];
    // per wx default cancel is not shown
    [[v cell] setCancelButtonCell:nil];

    wxNSSearchFieldControl* c = new wxNSSearchFieldControl( wxpeer, v );
    c->SetNeedsFrame( false );
    c->SetStringValue( str );
    return c;
}

#endif // wxUSE_NATIVE_SEARCH_CONTROL

#endif // wxUSE_SEARCHCTRL
