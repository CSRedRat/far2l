#include "stdafx.h"
#include <string>
#include <malloc.h>
#include <locale> 
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <mutex>
#include <condition_variable>
#include <wx/clipbrd.h>

#include "WinCompat.h"
#include "WinPort.h"
#include "WinPortHandle.h"
#include "Utils.h"

	
	
template<class RV, class FN>
	class Caller
{
	FN _fn;
	std::mutex _mutex;
	std::condition_variable _cond;
	RV _result;
	bool _done;
	
	
	protected:
	void Callback()
	{
			_result = _fn();
			std::unique_lock<std::mutex> locker(_mutex);
			_done = true;
			_cond.notify_all();
	}
	
	public:
	Caller(FN fn):_fn(fn) {
		
	}
	
	RV Do() {
		_done = false;
		wxTheApp->GetTopWindow()->GetEventHandler()->CallAfter(std::bind(&Caller::Callback, this));
		for (;;) {
			std::unique_lock<std::mutex> locker(_mutex);
			if (_done) break;
			_cond.wait(locker);
		}
		return _result;
	}
};

template <class RV, class FN> 
	RV CallInMain(FN fn)
{
	Caller<RV, FN> c(fn);
	return c.Do();
}



extern "C" {
	std::mutex g_clipboard_mutex;
	static class CustomFormats : std::map<UINT, std::wstring>  {
	public:
		CustomFormats() :_next_index(0)
		{
		}

		UINT Register(LPCWSTR lpszFormat)
		{
			for (auto i : *this) {
				if (i.second == lpszFormat)
					return i.first;
			}

			for (;;) {
				_next_index++;
				if (_next_index < 0xC000 || _next_index > 0xFFFF)
					_next_index = 0xC000;
				if (find(_next_index)==end()) {
					insert(value_type(_next_index, std::wstring(lpszFormat)));
					return _next_index;
				}
			}
		}


		UINT _next_index;
	} g_custom_formats;


	static struct CustomData : std::map<UINT, std::vector<char> >  {
	} g_custom_data;
	static std::set<PVOID> g_free_pendings;
	
	WINPORT_DECL(RegisterClipboardFormat, UINT, (LPCWSTR lpszFormat))
	{		
		std::lock_guard<std::mutex> lock(g_clipboard_mutex);
		return g_custom_formats.Register(lpszFormat);
	}


	WINPORT_DECL(OpenClipboard, BOOL, (PVOID Reserved))
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(OpenClipboard), Reserved);
			return CallInMain<BOOL>(fn);
		}
		fprintf(stderr, "OpenClipboard\n");
		BOOL rv = wxTheClipboard->Open() ? TRUE : FALSE;
		fprintf(stderr, "OpenClipboard rv=%i\n", rv);
		return rv;
	}

	WINPORT_DECL(CloseClipboard, BOOL, ())
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(CloseClipboard));
			return CallInMain<BOOL>(fn);
		}
		fprintf(stderr, "CloseClipboard\n");
		std::lock_guard<std::mutex> lock(g_clipboard_mutex);
		for (auto p : g_free_pendings) free(p);
		g_free_pendings.clear();
		wxTheClipboard->Flush();
		wxTheClipboard->Close();
		return TRUE;
	}
	WINPORT_DECL(EmptyClipboard, BOOL, ())
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(EmptyClipboard));
			return CallInMain<BOOL>(fn);
		}
		fprintf(stderr, "EmptyClipboard\n");
		wxTheClipboard->Clear();
		std::lock_guard<std::mutex> lock(g_clipboard_mutex);
		g_custom_data.clear();
		return TRUE;
	}

	WINPORT_DECL(IsClipboardFormatAvailable, BOOL, (UINT format))
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(IsClipboardFormatAvailable), format);
			return CallInMain<BOOL>(fn);
		}
		if (format!=CF_UNICODETEXT && format!=CF_TEXT) {
			std::lock_guard<std::mutex> lock(g_clipboard_mutex);
			CustomData::iterator i = g_custom_data.find(format);
			return (i==g_custom_data.end()) ? FALSE : TRUE;
		} else {
			return wxTheClipboard->IsSupported( wxDF_TEXT ) ? TRUE : FALSE;
		}
	}

	WINPORT_DECL(GetClipboardData, HANDLE, (UINT format))
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(GetClipboardData), format);
			return CallInMain<HANDLE>(fn);
		}
		PVOID p = NULL;		
		if (format!=CF_UNICODETEXT && format!=CF_TEXT) {
			std::lock_guard<std::mutex> lock(g_clipboard_mutex);
			CustomData::iterator i = g_custom_data.find(format);
			if (i==g_custom_data.end()) 
				return NULL;

			p = malloc(i->second.size());
			if (p) memcpy(p, &i->second[0], i->second.size());
		} else if (format==CF_UNICODETEXT) {
			wxTextDataObject data;
			wxTheClipboard->GetData( data );
			p = _wcsdup(data.GetText().wchar_str());
		} else if (format==CF_TEXT) {
			wxTextDataObject data;
			wxTheClipboard->GetData( data );
			p = _strdup(data.GetText().char_str());
		}

		if (p) {
			std::lock_guard<std::mutex> lock(g_clipboard_mutex);
			g_free_pendings.insert(p);
		}
		return p;
	}

	WINPORT_DECL(SetClipboardData, HANDLE, (UINT format, HANDLE mem))
	{
		if (!wxIsMainThread()) {
			auto fn = std::bind(WINPORT(SetClipboardData), format, mem);
			return CallInMain<HANDLE>(fn);
		}
#ifdef _WIN32
		size_t len = _msize(mem);
#else
		size_t len = malloc_usable_size(mem);
#endif
fprintf(stderr, "SetClipboardData\n");
		if (format!=CF_UNICODETEXT && format!=CF_TEXT) {
			std::lock_guard<std::mutex> lock(g_clipboard_mutex);
			std::vector<char> &data = g_custom_data[format];
			data.resize(len);
			if (len) memcpy(&data[0], mem, data.size());
		} else if (format==CF_UNICODETEXT) {
			wxTheClipboard->SetData( new wxTextDataObject((const wchar_t *)mem) );
		} else if (format==CF_TEXT) {
			wxTheClipboard->SetData( new wxTextDataObject((const char *)mem) );
		}

		if (mem) {
			std::lock_guard<std::mutex> lock(g_clipboard_mutex);
			g_free_pendings.insert(mem);
		}
		return mem;
	}
}
