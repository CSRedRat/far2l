#include "stdafx.h"
#include "ConsoleOutput.h"
#include "ConsoleInput.h"
#include "wxWinTranslations.h"
#include <wx/fontdlg.h>
#include <wx/fontenum.h>
#include <wx/textfile.h>
ConsoleOutput g_wx_con_out;
ConsoleInput g_wx_con_in;
enum
{
    TIMER_ID_PERIODIC = 10
};

unsigned int font_width = 8;
unsigned int font_height = 8;

extern "C" void WinPortInitRegistry();

struct EventWithRect : wxCommandEvent
{
	EventWithRect(const SMALL_RECT &rect_, wxEventType commandType = wxEVT_NULL, int winid = 0) 
		:wxCommandEvent(commandType, winid) , rect(rect_)
	{
	}

	virtual wxEvent *Clone() const { return new EventWithRect(*this); }

	SMALL_RECT rect;
};

///////////////////////////////////////////

wxDEFINE_EVENT(WX_CONSOLE_REFRESH, EventWithRect);
wxDEFINE_EVENT(WX_CONSOLE_WINDOW_MOVED, EventWithRect);
wxDEFINE_EVENT(WX_CONSOLE_RESIZED, wxCommandEvent);
wxDEFINE_EVENT(WX_CONSOLE_TITLE_CHANGED, wxCommandEvent);

//////////////////////////////////////////

class WinPortApp: public wxApp
{
public:
    virtual bool OnInit();
};

class WinPortFrame;

class WinPortPanel: public wxPanel, protected ConsoleOutputListener
{
public:
    WinPortPanel(WinPortFrame *frame, const wxPoint& pos, const wxSize& size);
	virtual ~WinPortPanel();
	void OnChar( wxKeyEvent& event );

protected: 
	virtual void OnConsoleOutputUpdated(const SMALL_RECT &area);
	virtual void OnConsoleOutputResized();
	virtual void OnConsoleOutputTitleChanged();
	virtual void OnConsoleOutputWindowMoved(bool absolute, COORD pos);
	void RefreshArea( const SMALL_RECT &area );
private:
	void OnTimerPeriodic(wxTimerEvent& event);	
	void OnWindowMoved( wxCommandEvent& event );
	void OnRefresh( wxCommandEvent& event );
	void OnConsoleResized( wxCommandEvent& event );
	void OnTitleChanged( wxCommandEvent& event );
	void OnKeyDown( wxKeyEvent& event );
	void OnKeyUp( wxKeyEvent& event );
	void OnPaint( wxPaintEvent& event );
    void OnEraseBackground( wxEraseEvent& event );
	void OnSize(wxSizeEvent &event);
    wxDECLARE_EVENT_TABLE();

	wxMemoryDC 	_white_rectangle;
	wxBitmap	_white_bitmap;
	WinPortFrame *_frame;
	wxTimer* _cursor_timer;
	bool _cursor_state;
	wxKeyEvent _last_skipped_keydown;
	wxFont _font;
	bool _delayed_init_done, _resize_pending;

	void UpdateLargestScreenSize();
};

///////////////////////////////////////////

class WinPortFrame: public wxFrame
{
public:
    WinPortFrame(const wxString& title, const wxPoint& pos, const wxSize& size);
	virtual ~WinPortFrame();
protected: 
private:
	WinPortPanel		*_panel;

	void OnChar( wxKeyEvent& event )
	{
		_panel->OnChar(event);
	}
	void OnPaint( wxPaintEvent& event ) {}
	void OnEraseBackground( wxEraseEvent& event ) {}
    wxDECLARE_EVENT_TABLE();
};


wxBEGIN_EVENT_TABLE(WinPortFrame, wxFrame)
	EVT_PAINT(WinPortFrame::OnPaint)
	EVT_ERASE_BACKGROUND(WinPortFrame::OnEraseBackground)
	EVT_CHAR(WinPortFrame::OnChar)
wxEND_EVENT_TABLE()


////////////////////////////



wxBEGIN_EVENT_TABLE(WinPortPanel, wxPanel)
	EVT_TIMER(TIMER_ID_PERIODIC, WinPortPanel::OnTimerPeriodic)
	EVT_COMMAND(wxID_ANY, WX_CONSOLE_REFRESH, WinPortPanel::OnRefresh)
	EVT_COMMAND(wxID_ANY, WX_CONSOLE_WINDOW_MOVED, WinPortPanel::OnWindowMoved)
	EVT_COMMAND(wxID_ANY, WX_CONSOLE_RESIZED, WinPortPanel::OnConsoleResized)
	EVT_COMMAND(wxID_ANY, WX_CONSOLE_TITLE_CHANGED, WinPortPanel::OnTitleChanged)
	EVT_KEY_DOWN(WinPortPanel::OnKeyDown)
	EVT_KEY_UP(WinPortPanel::OnKeyUp)
	EVT_CHAR(WinPortPanel::OnChar)
	EVT_PAINT(WinPortPanel::OnPaint)
	EVT_ERASE_BACKGROUND(WinPortPanel::OnEraseBackground)
	EVT_SIZE(WinPortPanel::OnSize)
wxEND_EVENT_TABLE()

wxIMPLEMENT_APP_NO_MAIN(WinPortApp);


class WinPortAppThread : public wxThread
{
public:
	WinPortAppThread(int argc, char **argv, int(*appmain)(int argc, char **argv))
		: wxThread(wxTHREAD_DETACHED), _argv(argv), _argc(argc), _appmain(appmain)  {  }

protected:
	virtual ExitCode Entry()
	{
		_r = _appmain(_argc, _argv);
		exit(_r);
		return 0;
	}

private:
	char **_argv;
	int _argc;
	int(*_appmain)(int argc, char **argv);
	int _r;
} *g_winport_app_thread = NULL;

extern "C" int WinPortMain(int argc, char **argv, int(*AppMain)(int argc, char **argv))
{
	wxInitialize();
	WinPortInitRegistry();
	g_wx_con_out.WriteString(L"Hello", 5);
	if (AppMain && !g_winport_app_thread) {
		g_winport_app_thread = new WinPortAppThread(argc, argv, AppMain);
		if (!g_winport_app_thread)
			return -1;		
	}

	wxEntry(argc, argv);
	wxUninitialize();
	return 0;
}


bool WinPortApp::OnInit()
{
	WinPortFrame *frame = new WinPortFrame("WinPortApp", wxDefaultPosition, wxDefaultSize );
    //WinPortFrame *frame = new WinPortFrame( "WinPortApp", wxPoint(50, 50), wxSize(450, 340) );
    frame->Show( true );

	unsigned int width = 80, height = 25;
	g_wx_con_out.GetSize(width, height);
	width*= font_width; height*= font_height;
	frame->SetClientSize(width, height);
	
	if (g_winport_app_thread) {
		WinPortAppThread *tmp = g_winport_app_thread;
		g_winport_app_thread = NULL;
		if (tmp->Run() != wxTHREAD_NO_ERROR)
			delete tmp;
	}
	
    return true;
}


WinPortFrame::WinPortFrame(const wxString& title, const wxPoint& pos, const wxSize& size)
        : wxFrame(NULL, wxID_ANY, title, pos, size)
{
	_panel = new WinPortPanel(this, wxPoint(0, 0), GetClientSize());
	_panel->SetFocus();
	SetBackgroundColour(*wxBLACK);
}

WinPortFrame::~WinPortFrame()
{
	delete _panel;
	_panel = NULL;
}


///////////////////////////
class FixedFontLookup : wxFontEnumerator
{
	wxString _result;
	virtual bool OnFacename(const wxString &font)
	{
		_result = font;
		return false;
	}
public:

	wxString Query()
	{
		_result.clear();
		EnumerateFacenames(wxFONTENCODING_SYSTEM, true);
		fprintf(stderr, "FixedFontLookup: %ls\n", (const wchar_t *)_result);
		return _result;
	}
	
	
};

void InitializeFont(wxFrame *frame, wxFont& font)
{
	std::string path = getenv("HOME");
	path+= "/.WinPort";
	mkdir(path.c_str(), 0777);
	path+= "/font";
	wxTextFile file(path);
	if (file.Exists() && file.Open()) {
		for (wxString str = file.GetFirstLine(); !file.Eof(); str = file.GetNextLine()) {
			font.SetNativeFontInfo(str);
			if (font.IsOk()) {
				printf("InitializeFont: used %ls\n", (const wchar_t *)str);
				return;				
			}
		}
	} else 
		file.Create(path);
	
	for (;;) {
		FixedFontLookup ffl;
		wxString fixed_font = ffl.Query();
		if (!fixed_font.empty()) {
			font = wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, fixed_font);
		}
		if (fixed_font.empty() || !font.IsOk())
			font = wxFont(wxSystemSettings::GetFont(wxSYS_ANSI_FIXED_FONT));
		font = wxGetFontFromUser(frame, font);
		if (font.IsOk()) {
			file.InsertLine(font.GetNativeFontInfoDesc(), 0);
			file.Write();
			return;
		}
	}
}


WinPortPanel::WinPortPanel(WinPortFrame *frame, const wxPoint& pos, const wxSize& size)
        : wxPanel(frame, wxID_ANY, pos, size, wxWANTS_CHARS | wxNO_BORDER), 
		_white_bitmap(48, 48,  wxBITMAP_SCREEN_DEPTH), _frame(frame), _cursor_timer(NULL),  
		_cursor_state(false), _delayed_init_done(false), _resize_pending(false)
{
	SetBackgroundColour(*wxBLACK);
	InitializeFont(frame, _font);
	_white_rectangle.SelectObject(_white_bitmap);	
	wxBrush* brush = wxTheBrushList->FindOrCreateBrush(wxColor(0xff, 0xff, 0xff));
	_white_rectangle.SetBrush(*brush);
	_white_rectangle.DrawRectangle(0, 0,  _white_bitmap.GetWidth() ,_white_bitmap.GetHeight());
	
	
	_white_rectangle.SetFont(_font);
	fprintf(stderr, "Font: '%ls' %s\n", (const wchar_t *)_font.GetFaceName(), _font.IsFixedWidth() ? "monospaced" : "not monospaced");
	
	wchar_t test_char[2] = {0};
	for(test_char[0] = L'A';  test_char[0]<=L'Z'; ++test_char[0]) {
		wxSize char_size = _white_rectangle.GetTextExtent(test_char);
		if (font_width < char_size.GetWidth()) font_width = char_size.GetWidth();
		if (font_height < char_size.GetHeight()) font_height = char_size.GetHeight();
	}
	//font_height+= font_height/4;

	
	g_wx_con_out.SetListener(this);
	_cursor_timer = new wxTimer(this, TIMER_ID_PERIODIC);
	_cursor_timer->Start(500);
	OnConsoleOutputTitleChanged();
	UpdateLargestScreenSize();	
}

WinPortPanel::~WinPortPanel()
{
	delete _cursor_timer;
	g_wx_con_out.SetListener(NULL);
}


void WinPortPanel::UpdateLargestScreenSize()
{
	wxDisplay disp(wxDisplay::GetFromWindow	(_frame));
	wxRect rc = disp.GetClientArea();
	wxSize outer_size = _frame->GetSize();
	wxSize inner_size = GetClientSize();
	rc.SetWidth(rc.GetWidth() - (outer_size.GetWidth() - inner_size.GetWidth()));
	rc.SetHeight(rc.GetHeight() - (outer_size.GetHeight() - inner_size.GetHeight()));
	COORD size = {(SHORT)(rc.GetWidth() / font_width), (SHORT)(rc.GetHeight() / font_height)};
	g_wx_con_out.SetLargestConsoleWindowSize(size);
}

void WinPortPanel::OnTimerPeriodic(wxTimerEvent& event)
{
	if (!_delayed_init_done) {
		_delayed_init_done = true;
		UpdateLargestScreenSize();
	}

	if (_resize_pending) {
		_resize_pending = false;
		DWORD conmode = 0;
		if (WINPORT(GetConsoleMode)(NULL, &conmode) && (conmode&ENABLE_WINDOW_INPUT)!=0) {
			unsigned int prev_width = 0, prev_height = 0;
			g_wx_con_out.GetSize(prev_width, prev_height);
	
			int width = 0, height = 0;
			_frame->GetClientSize(&width, &height);
			width/= font_width; 
			height/= font_height;
			if (width!=(int)prev_width || height!=(int)prev_height) {
				fprintf(stderr, "Changing size: %u x %u\n", width, height);
				g_wx_con_out.SetSize(width, height);
				INPUT_RECORD ir = {0};
				ir.EventType = WINDOW_BUFFER_SIZE_EVENT;
				ir.Event.WindowBufferSizeEvent.dwSize.X = width;
				ir.Event.WindowBufferSizeEvent.dwSize.Y = height;
				g_wx_con_in.Enqueue(&ir, 1);
			}
	
			Refresh(false);
			UpdateLargestScreenSize();
		}
	}
	
	
	_cursor_state = !_cursor_state;
	const COORD &pos = g_wx_con_out.GetCursor();
	SMALL_RECT area = {pos.X, pos.Y, pos.X, pos.Y};
	RefreshArea( area );
}

void WinPortPanel::OnConsoleOutputUpdated(const SMALL_RECT &area)
{
	wxCommandEvent *event = new EventWithRect(area, WX_CONSOLE_REFRESH);
	if (event)
		wxQueueEvent	(this, event);
}

void WinPortPanel::OnConsoleOutputResized()
{
	wxCommandEvent *event = new wxCommandEvent(WX_CONSOLE_RESIZED);
	if (event)
		wxQueueEvent	(this, event);
}

void WinPortPanel::OnConsoleOutputTitleChanged()
{
	wxCommandEvent *event = new wxCommandEvent(WX_CONSOLE_TITLE_CHANGED);
	if (event)
		wxQueueEvent	(this, event);
}

void WinPortPanel::OnConsoleOutputWindowMoved(bool absolute, COORD pos)
{
	SMALL_RECT rect = {pos.X, pos.Y, absolute ? 1 : 0, 0};
	wxCommandEvent *event = new EventWithRect(rect, WX_CONSOLE_WINDOW_MOVED);
	if (event)
		wxQueueEvent	(this, event);
}

void WinPortPanel::RefreshArea( const SMALL_RECT &area )
{
	wxRect rc;
	rc.SetLeft(((int)area.Left) * font_width);
	rc.SetRight(((int)area.Right + 1) * font_width);
	rc.SetTop(((int)area.Top) * font_height);
	rc.SetBottom(((int)area.Bottom + 1) * font_height);
	Refresh(false, &rc);
}

void WinPortPanel::OnWindowMoved( wxCommandEvent& event )
{
	EventWithRect *e = (EventWithRect *)&event;
	int x = e->rect.Left, y = e->rect.Top;
	x*= font_width; y*= font_height;
	if (!e->rect.Right) {
		int dx = 0, dy = 0;
		GetPosition(&dx, &dy);
		x+= dx; y+= dy;
	}
	Move(x, y);
	UpdateLargestScreenSize();
}

void WinPortPanel::OnRefresh( wxCommandEvent& event )
{
	EventWithRect *e = (EventWithRect *)&event;
	RefreshArea( e->rect );
}

void WinPortPanel::OnConsoleResized( wxCommandEvent& event )
{
	unsigned int width = 0, height = 0;
	g_wx_con_out.GetSize(width, height);
	width*= font_width;
	height*= font_height;
	int prev_width = 0, prev_height = 0;
	_frame->GetClientSize(&prev_width, &prev_height);
	if (width > prev_width || height > prev_height)
		_frame->SetPosition(wxPoint(0,0 ));
	
	_frame->SetClientSize(width, height);
	Refresh(false);
	UpdateLargestScreenSize();
}

void WinPortPanel::OnTitleChanged( wxCommandEvent& event )
{
	const std::wstring &title = g_wx_con_out.GetTitle();
	wxGetApp().SetAppDisplayName(title.c_str());
	_frame->SetTitle(title.c_str());
}

void WinPortPanel::OnKeyDown( wxKeyEvent& event )
{
	fprintf(stderr, "OnKeyDown: %x %x\n", event.GetUnicodeKey(), event.GetKeyCode());
	if (event.GetKeyCode()==WXK_RETURN && event.AltDown() &&
		!event.ShiftDown() && !event.ControlDown() && !event.MetaDown()) {
		_frame->ShowFullScreen(!_frame->IsFullScreen());
		event.Skip();
		_resize_pending = true;
		return;
	}
	
	wx2INPUT_RECORD ir(event, TRUE);
	if (event.GetUnicodeKey()==WXK_NONE&& event.GetKeyCode()!=WXK_DELETE)
		g_wx_con_in.Enqueue(&ir, 1);
	else 
		_last_skipped_keydown = event;
	event.Skip();
}

void WinPortPanel::OnKeyUp( wxKeyEvent& event )
{
	wx2INPUT_RECORD ir(event, FALSE);
	fprintf(stderr, "OnKeyUp: %x %x\n", event.GetUnicodeKey(), event.GetKeyCode());
	if (event.GetUnicodeKey()==WXK_NONE && event.GetKeyCode()!=WXK_DELETE) 
		g_wx_con_in.Enqueue(&ir, 1);
	//event.Skip();
}

void WinPortPanel::OnChar( wxKeyEvent& event )
{
	fprintf(stderr, "OnChar: %x %x\n", event.GetUnicodeKey(), event.GetKeyCode());
	if (event.GetUnicodeKey()!=WXK_NONE || event.GetKeyCode()==WXK_DELETE) {
		wx2INPUT_RECORD ir_down(_last_skipped_keydown, TRUE);
		wx2INPUT_RECORD ir_up(_last_skipped_keydown, FALSE);
		wx2INPUT_RECORD ir_char(event, TRUE);
		ir_down.Event.KeyEvent.uChar = 
			ir_up.Event.KeyEvent.uChar = 
				ir_char.Event.KeyEvent.uChar;
//		fprintf(stderr, "OnChar: %wc\n", event.GetUnicodeKey());

		g_wx_con_in.Enqueue(&ir_down, 1);
		g_wx_con_in.Enqueue(&ir_up, 1);
	}
	//event.Skip();
}

unsigned int DivCeil(unsigned int v, unsigned int d)
{
	unsigned int r = v / d;
	return (r * d == v) ? r : r + 1;
}

class ConsoleColorChanger
{
	wxPaintDC &_dc;
	class RememberedColor
	{
		wxColour _clr;
		bool _valid;

	public:
		RememberedColor() : _valid(false) {}
		bool Change(wxColour clr)
		{
			if (!_valid || _clr!=clr) {
				_valid = true;
				_clr = clr;
				return true;
			}
			return false;
		}
	}_brush, _text_fg; // _text_bg;

public:
	ConsoleColorChanger(wxPaintDC &dc) 
		: _dc(dc)
	{
	}
	
	void SetBackgroundAttributes(unsigned short attributes)
	{
		wxColour clr = ConsoleBackground2wxColor(attributes);
		if (_brush.Change(clr)) {
			wxBrush* brush = wxTheBrushList->FindOrCreateBrush(clr);
			_dc.SetBrush(*brush);
			_dc.SetBackground(*brush);
		}
	}

	void SetTextAttributes(unsigned short attributes)
	{
		wxColour clr = ConsoleForeground2wxColor(attributes);
		if (_text_fg.Change(clr))
			_dc.SetTextForeground(clr);

		//clr = ConsoleBackground2wxColor(attributes);
		//if (_text_bg.Change(clr))
		//	_dc.SetTextBackground(clr);
	}
};


void WinPortPanel::OnPaint( wxPaintEvent& event )
{
	wxPaintDC dc(this);
	unsigned int cw, ch; g_wx_con_out.GetSize(cw, ch);
	wxRegion rgn = GetUpdateRegion();
	wxRect box = rgn.GetBox();
	SMALL_RECT area = {(unsigned int) (box.GetLeft() / font_width), (unsigned int) (box.GetTop() / font_height),
		DivCeil(box.GetRight(), font_width), DivCeil(box.GetBottom(), font_height)};
	if (area.Right >= cw) area.Right = cw - 1;
	if (area.Bottom >= ch) area.Bottom = ch - 1;
	if (area.Right < area.Left || area.Bottom < area.Top) return;
	
	std::vector<CHAR_INFO> line(cw);
	wxPen *trans_pen = wxThePenList->FindOrCreatePen(wxColour(0, 0, 0), 1, wxPENSTYLE_TRANSPARENT);
	dc.SetPen(*trans_pen);
	dc.SetBackgroundMode(wxPENSTYLE_TRANSPARENT);
	dc.SetFont(_font);
	ConsoleColorChanger ccc(dc);
	for (unsigned int cy = area.Top; cy <= area.Bottom; ++cy) {
		COORD data_size = {cw, 1};
		COORD data_pos = {0, 0};
		SMALL_RECT screen_rect = {area.Left, cy, area.Right, cy};

		if (rgn.Contains(0, cy * font_height, cw * font_width, font_height)==wxOutRegion) continue;
		g_wx_con_out.Read(&line[area.Left], data_size, data_pos, screen_rect);
		
		for (unsigned int cx = area.Left; cx <= area.Right; ++cx) {
			if (rgn.Contains(cx * font_width, cy * font_height, font_width, font_height)==wxOutRegion) continue;

			unsigned short attributes = line[cx].Attributes;
			ccc.SetBackgroundAttributes(attributes);
			unsigned int x = cx * font_width;
			unsigned int y = cy * font_height;
			//int fill_x = x, fill_y = y, fill_width = font_width, fill_height = font_height;
			//if (fill_x < font_width) { fill_width+= fill_x; fill_x = 0; }
			//if (fill_y < font_height) { fill_height+= fill_y; fill_y = 0; };
			
			dc.DrawRectangle(x, y, font_width, font_height);
			
			WCHAR sz[2] = {line[cx].Char.UnicodeChar, 0};
			if (sz[0] && sz[0]!=L' ') {
				ccc.SetTextAttributes(attributes);
				dc.DrawText(sz, x, y);
			}
		}
	}

	if (_cursor_state) {
		UCHAR height;
		bool visible;
		const COORD &pos = g_wx_con_out.GetCursor(height, visible);
		if (visible) {
			unsigned int w = font_width;
			unsigned int h = (font_height * height) / 100;
			if (h==0) h = 1;
			unsigned int x = pos.X * font_width;
			unsigned int y = (pos.Y + 1) * font_height - h;	
			if (rgn.Contains(x, y, w, h)!=wxOutRegion)  {
				dc.Blit(x, y, w, h, &_white_rectangle, 0, 0, wxCOPY);
			}
		}
	}
}

void WinPortPanel::OnEraseBackground( wxEraseEvent& event )
{
}

void WinPortPanel::OnSize(wxSizeEvent &event)
{
	UpdateLargestScreenSize();
	_resize_pending = true;	
}
