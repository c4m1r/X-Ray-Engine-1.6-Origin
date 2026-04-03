//----------------------------------------------------
// file: FileSystem.cpp
//----------------------------------------------------

#include "stdafx.h"
#pragma hdrstop

#include "cderr.h"
#include "commdlg.h"
#include "vfw.h"
#include <ShObjIdl.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

EFS_Utils*	xr_EFS	= NULL;

namespace
{
	void DialogLog(const char* fmt, ...)
	{
		char buffer[2048] = {};
		va_list args;
		va_start(args, fmt);
		vsnprintf(buffer, sizeof(buffer), fmt, args);
		va_end(args);
		Log(buffer);
		OutputDebugStringA(buffer);
		OutputDebugStringA("\n");
	}

	void DialogLogHr(const char* stage, HRESULT hr)
	{
		DialogLog("! %s hr=0x%08X", stage, static_cast<unsigned>(hr));
	}

	HWND GetDialogOwnerWindow()
	{
		HWND hwnd = GetActiveWindow();
		if (!hwnd)
			hwnd = GetForegroundWindow();
		if (hwnd)
		{
			HWND root = GetAncestor(hwnd, GA_ROOTOWNER);
			if (root)
				hwnd = root;
		}
		return hwnd;
	}

	std::wstring ToWide(LPCSTR text)
	{
		if (!text || !text[0])
			return std::wstring();

		const int len = MultiByteToWideChar(CP_ACP, 0, text, -1, 0, 0);
		if (len <= 1)
			return std::wstring();

		std::wstring out;
		out.resize(len);
		MultiByteToWideChar(CP_ACP, 0, text, -1, &out[0], len);
		out.resize(len - 1);
		return out;
	}

	xr_string ToAnsi(const wchar_t* text)
	{
		if (!text || !text[0])
			return xr_string();

		const int len = WideCharToMultiByte(CP_ACP, 0, text, -1, 0, 0, 0, 0);
		if (len <= 1)
			return xr_string();

		xr_string out;
		out.resize(len);
		WideCharToMultiByte(CP_ACP, 0, text, -1, &out[0], len, 0, 0);
		out.resize(len - 1);
		return out;
	}

	xr_string NormalizeDefaultExtension(LPCSTR def_ext)
	{
		if (!def_ext || !def_ext[0])
			return xr_string();

		string_path token = {};
		_GetItem(def_ext, 0, token, ';');
		xr_string ext = token;

		while (!ext.empty() && (ext[0] == '*' || ext[0] == '.'))
			ext.erase(ext.begin());

		return ext;
	}

	void BuildFilterSpecs(
		LPCSTR info,
		LPCSTR ext,
		std::vector<std::wstring>& names,
		std::vector<std::wstring>& patterns,
		std::vector<COMDLG_FILTERSPEC>& specs
	)
	{
		names.clear();
		patterns.clear();
		specs.clear();

		if (!ext || !ext[0])
		{
			names.push_back(ToWide("All files (*.*)"));
			patterns.push_back(ToWide("*.*"));
		}
		else
		{
			const xr_string caption = info && info[0] ? xr_string(info) : xr_string("Files");
			const int count = _GetItemCount(ext, ';');
			if (count <= 1)
			{
				xr_string label = caption;
				label += " (";
				label += ext;
				label += ")";
				names.push_back(ToWide(label.c_str()));
				patterns.push_back(ToWide(ext));
			}
			else
			{
				xr_string combined_label = caption;
				combined_label += " (";
				combined_label += ext;
				combined_label += ")";
				names.push_back(ToWide(combined_label.c_str()));
				patterns.push_back(ToWide(ext));

				for (int idx = 0; idx < count; ++idx)
				{
					string64 item = {};
					_GetItem(ext, idx, item, ';');
					xr_string label = caption;
					label += " (";
					label += item;
					label += ")";
					names.push_back(ToWide(label.c_str()));
					patterns.push_back(ToWide(item));
				}
			}
		}

		specs.reserve(names.size());
		for (size_t i = 0; i < names.size(); ++i)
		{
			COMDLG_FILTERSPEC spec = {};
			spec.pszName = names[i].c_str();
			spec.pszSpec = patterns[i].c_str();
			specs.push_back(spec);
		}
	}

	HRESULT CreateShellItemFromAnsiPath(LPCSTR path, IShellItem** item)
	{
		*item = 0;
		if (!path || !path[0])
			return S_FALSE;

		const DWORD attrs = GetFileAttributesA(path);
		if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY))
			return S_FALSE;

		std::wstring wide = ToWide(path);
		if (wide.empty())
			return E_INVALIDARG;

		return SHCreateItemFromParsingName(wide.c_str(), 0, IID_PPV_ARGS(item));
	}

	void PrepareInitialPath(FS_Path& P, LPSTR buffer, int sz_buf)
	{
		if (!buffer || !buffer[0])
			return;

		string_path dr = {};
		if (buffer[0] == '\\' && buffer[1] == '\\')
			return;

		_splitpath(buffer, dr, 0, 0, 0);
		if (0 == dr[0])
		{
			string_path full = {};
			P._update(full, buffer);
			xr_strcpy(buffer, sz_buf, full);
		}
	}

	void SetDialogFolderAndName(IFileDialog* dialog, FS_Path& P, LPCSTR offset, LPCSTR buffer)
	{
		string512 initial_dir = {};
		xr_strcpy(initial_dir, (offset && offset[0]) ? offset : P.m_Path);

		IShellItem* folder = 0;
		HRESULT hr = CreateShellItemFromAnsiPath(initial_dir, &folder);
		if (SUCCEEDED(hr) && folder)
		{
			hr = dialog->SetDefaultFolder(folder);
			if (FAILED(hr))
				DialogLogHr("IFileDialog::SetDefaultFolder", hr);
			hr = dialog->SetFolder(folder);
			if (FAILED(hr))
				DialogLogHr("IFileDialog::SetFolder", hr);
			folder->Release();
		}
		else if (FAILED(hr))
		{
		}

		if (buffer && buffer[0])
		{
			string_path fname = {};
			string_path ext = {};
			_splitpath(buffer, 0, 0, fname, ext);
			xr_string full_name = fname;
			full_name += ext;
			if (!full_name.empty())
			{
				std::wstring wide_name = ToWide(full_name.c_str());
				if (!wide_name.empty())
				{
					hr = dialog->SetFileName(wide_name.c_str());
					if (FAILED(hr))
						DialogLogHr("IFileDialog::SetFileName", hr);
				}
			}
		}
	}

	bool CopyResultToBuffer(const xr_string& value, LPSTR buffer, int sz_buf)
	{
		if (value.size() + 1 > static_cast<size_t>(sz_buf))
		{
			DialogLog("! Dialog result too long: size=%u limit=%d", static_cast<unsigned>(value.size()), sz_buf);
			return false;
		}

		xr_strcpy(buffer, sz_buf, value.c_str());
		strlwr(buffer);
		return true;
	}
}
//----------------------------------------------------
EFS_Utils::EFS_Utils( )
{
}

EFS_Utils::~EFS_Utils()
{
}

xr_string	EFS_Utils::ExtractFileName(LPCSTR src)
{
	string_path name;
	_splitpath	(src,0,0,name,0);
    return xr_string(name);
}

xr_string	EFS_Utils::ExtractFileExt(LPCSTR src)
{
	string_path ext;
	_splitpath	(src,0,0,0,ext);
    return xr_string(ext);
}

xr_string	EFS_Utils::ExtractFilePath(LPCSTR src)
{
	string_path drive,dir;
	_splitpath	(src,drive,dir,0,0);
    return xr_string(drive)+dir;
}

xr_string	EFS_Utils::ExcludeBasePath(LPCSTR full_path, LPCSTR excl_path)
{
    LPCSTR sub		= strstr(full_path,excl_path);
	if (0!=sub) 	return xr_string(sub+xr_strlen(excl_path));
	else	   		return xr_string(full_path);
}

xr_string	EFS_Utils::ChangeFileExt(LPCSTR src, LPCSTR ext)
{
	/*xr_string	tmp;
	LPSTR src_ext	= strext(src);
    if (src_ext){
	    size_t		ext_pos	= src_ext-src;
        tmp.assign	(src,0,ext_pos);
    }else{
        tmp			= src;
    }
    tmp				+= ext;
	return tmp;*/
    xr_string	tmp;
	LPSTR src_ext	= strext(src);
    if (src_ext){
		size_t		ext_pos	= src_ext-src;
		xr_string _src(src);
		tmp = _src.substr(0,ext_pos);
	}else{
        tmp			= src;
    }
    tmp				+= ext;
	return tmp;
}

xr_string	EFS_Utils::ChangeFileExt(const xr_string& src, LPCSTR ext)
{
	return ChangeFileExt(src.c_str(),ext);
}

//----------------------------------------------------
void MakeFilter(string1024& dest, LPCSTR info, LPCSTR ext)
{
    std::string res;

    if (ext)
    {
    	res += info;
		res	+= "(";
		res	+= ext;
		res	+= ")|";
		res	+= ext;
		res	+= "|";
        int icnt		= _GetItemCount(ext,';');
        if(icnt>1)
        {
        for(int idx=0; idx<icnt; ++idx)
        {
          string64		buf;
          _GetItem		(ext, idx, buf, ';');
    
          res += info;
          res += "(";
          res += buf;
          res += ")|";
          res += buf;
          res += "|";
        }
      }
    	res += "|";
	}else
    {
    	res = "All files(*.*)|*.*||";
    }
    xr_strcpy(dest, res.c_str());
    
    for(u32 i=0; i<res.size(); ++i)           
    {
    	if(res[i]=='|')
        	dest[i]='\0';
    }
  

}

//------------------------------------------------------------------------------
// start_flt_ext = -1-all 0..n-indices
//------------------------------------------------------------------------------
  
// Vista uses this hook for old-style save dialog
UINT_PTR CALLBACK OFNHookProcOldStyle(HWND , UINT , WPARAM , LPARAM )
{
	// let default hook work on this message
	return 0;
}

bool EFS_Utils::GetOpenNameInternal( LPCSTR initial,  LPSTR buffer, int sz_buf, bool bMulti, LPCSTR offset, int start_flt_ext )
{
	VERIFY				(buffer&&(sz_buf>0));
	FS_Path& P			= *FS.get_path(initial);
	DialogLog("* GetOpenNameInternal begin initial=%s multi=%d start_flt_ext=%d buffer=%s",
		initial ? initial : "<null>",
		bMulti ? 1 : 0,
		start_flt_ext,
		buffer[0] ? buffer : "<empty>");

	PrepareInitialPath(P, buffer, sz_buf);

	HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	bool need_uninit = SUCCEEDED(hr);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		DialogLogHr("CoInitializeEx(open)", hr);
		return false;
	}
	DialogLog("* CoInitializeEx(open) hr=0x%08X", static_cast<unsigned>(hr));

	IFileOpenDialog* dialog = 0;
	hr = CoCreateInstance(CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
	if (FAILED(hr))
	{
		DialogLogHr("CoCreateInstance(IFileOpenDialog)", hr);
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	DWORD options = 0;
	hr = dialog->GetOptions(&options);
	if (FAILED(hr))
		DialogLogHr("IFileOpenDialog::GetOptions", hr);
	options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
	if (bMulti)
		options |= FOS_ALLOWMULTISELECT;
	hr = dialog->SetOptions(options);
	if (FAILED(hr))
		DialogLogHr("IFileOpenDialog::SetOptions", hr);

	std::vector<std::wstring> names;
	std::vector<std::wstring> patterns;
	std::vector<COMDLG_FILTERSPEC> specs;
	BuildFilterSpecs(P.m_FilterCaption ? P.m_FilterCaption : "", P.m_DefExt, names, patterns, specs);
	if (!specs.empty())
	{
		hr = dialog->SetFileTypes(static_cast<UINT>(specs.size()), &specs[0]);
		if (FAILED(hr))
			DialogLogHr("IFileOpenDialog::SetFileTypes", hr);

		UINT filter_index = 1;
		if (start_flt_ext >= 0)
		{
			filter_index = static_cast<UINT>(start_flt_ext + 2);
			if (filter_index > specs.size())
				filter_index = static_cast<UINT>(specs.size());
		}
		hr = dialog->SetFileTypeIndex(filter_index);
		if (FAILED(hr))
			DialogLogHr("IFileOpenDialog::SetFileTypeIndex", hr);
	}

	hr = dialog->SetTitle(L"Open a File");
	if (FAILED(hr))
		DialogLogHr("IFileOpenDialog::SetTitle", hr);

	SetDialogFolderAndName(dialog, P, offset, buffer);

	HWND owner = GetDialogOwnerWindow();
	DialogLog("* IFileOpenDialog::Show owner=0x%p", owner);
	hr = dialog->Show(owner);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
	{
		DialogLog("* IFileOpenDialog cancelled");
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}
	if (FAILED(hr))
	{
		DialogLogHr("IFileOpenDialog::Show", hr);
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	xr_string result;
	if (bMulti)
	{
		IShellItemArray* items = 0;
		hr = dialog->GetResults(&items);
		if (FAILED(hr))
		{
			DialogLogHr("IFileOpenDialog::GetResults", hr);
			dialog->Release();
			if (need_uninit)
				CoUninitialize();
			return false;
		}

		DWORD count = 0;
		hr = items->GetCount(&count);
		if (FAILED(hr))
			DialogLogHr("IShellItemArray::GetCount", hr);

		for (DWORD i = 0; SUCCEEDED(hr) && i < count; ++i)
		{
			IShellItem* item = 0;
			hr = items->GetItemAt(i, &item);
			if (FAILED(hr))
			{
				DialogLogHr("IShellItemArray::GetItemAt", hr);
				break;
			}

			PWSTR path = 0;
			hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
			if (FAILED(hr))
			{
				DialogLogHr("IShellItem::GetDisplayName", hr);
				item->Release();
				break;
			}

			xr_string ansi = ToAnsi(path);
			if (!result.empty())
				result += ",";
			result += ansi;
			CoTaskMemFree(path);
			item->Release();
		}
		items->Release();
	}
	else
	{
		IShellItem* item = 0;
		hr = dialog->GetResult(&item);
		if (FAILED(hr))
		{
			DialogLogHr("IFileOpenDialog::GetResult", hr);
			dialog->Release();
			if (need_uninit)
				CoUninitialize();
			return false;
		}

		PWSTR path = 0;
		hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
		if (FAILED(hr))
		{
			DialogLogHr("IShellItem::GetDisplayName", hr);
			item->Release();
			dialog->Release();
			if (need_uninit)
				CoUninitialize();
			return false;
		}

		result = ToAnsi(path);
		CoTaskMemFree(path);
		item->Release();
	}

	dialog->Release();
	if (need_uninit)
		CoUninitialize();

	if (result.empty())
	{
		DialogLog("! IFileOpenDialog returned empty result");
		return false;
	}

	DialogLog("* IFileOpenDialog result=%s", result.c_str());
	return CopyResultToBuffer(result, buffer, sz_buf);
}

bool EFS_Utils::GetSaveName( LPCSTR initial, string_path& buffer, LPCSTR offset, int start_flt_ext )
{
	FS_Path& P			= *FS.get_path(initial);
	DialogLog("* GetSaveName begin initial=%s start_flt_ext=%d buffer=%s",
		initial ? initial : "<null>",
		start_flt_ext,
		buffer[0] ? buffer : "<empty>");

	PrepareInitialPath(P, buffer, sizeof(string_path));

	HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	bool need_uninit = SUCCEEDED(hr);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		DialogLogHr("CoInitializeEx(save)", hr);
		return false;
	}
	DialogLog("* CoInitializeEx(save) hr=0x%08X", static_cast<unsigned>(hr));

	IFileSaveDialog* dialog = 0;
	hr = CoCreateInstance(CLSID_FileSaveDialog, 0, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog));
	if (FAILED(hr))
	{
		DialogLogHr("CoCreateInstance(IFileSaveDialog)", hr);
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	DWORD options = 0;
	hr = dialog->GetOptions(&options);
	if (FAILED(hr))
		DialogLogHr("IFileSaveDialog::GetOptions", hr);
	options |= FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT;
	hr = dialog->SetOptions(options);
	if (FAILED(hr))
		DialogLogHr("IFileSaveDialog::SetOptions", hr);

	LPCSTR def_ext = P.m_DefExt;
	if (false)
	{
		if (strstr(P.m_DefExt, "*."))
			def_ext = strstr(P.m_DefExt, "*.") + 2;
	}

	std::vector<std::wstring> names;
	std::vector<std::wstring> patterns;
	std::vector<COMDLG_FILTERSPEC> specs;
	BuildFilterSpecs(P.m_FilterCaption ? P.m_FilterCaption : "", def_ext, names, patterns, specs);
	if (!specs.empty())
	{
		hr = dialog->SetFileTypes(static_cast<UINT>(specs.size()), &specs[0]);
		if (FAILED(hr))
			DialogLogHr("IFileSaveDialog::SetFileTypes", hr);

		UINT filter_index = 1;
		if (start_flt_ext >= 0)
		{
			filter_index = static_cast<UINT>(start_flt_ext + 2);
			if (filter_index > specs.size())
				filter_index = static_cast<UINT>(specs.size());
		}
		hr = dialog->SetFileTypeIndex(filter_index);
		if (FAILED(hr))
			DialogLogHr("IFileSaveDialog::SetFileTypeIndex", hr);
	}

	xr_string normalized_ext = NormalizeDefaultExtension(def_ext);
	if (!normalized_ext.empty())
	{
		std::wstring wide_ext = ToWide(normalized_ext.c_str());
		hr = dialog->SetDefaultExtension(wide_ext.c_str());
		if (FAILED(hr))
			DialogLogHr("IFileSaveDialog::SetDefaultExtension", hr);
	}

	hr = dialog->SetTitle(L"Save a File");
	if (FAILED(hr))
		DialogLogHr("IFileSaveDialog::SetTitle", hr);

	SetDialogFolderAndName(dialog, P, offset, buffer);

	HWND owner = GetDialogOwnerWindow();
	DialogLog("* IFileSaveDialog::Show owner=0x%p", owner);
	hr = dialog->Show(owner);
	if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
	{
		DialogLog("* IFileSaveDialog cancelled");
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}
	if (FAILED(hr))
	{
		DialogLogHr("IFileSaveDialog::Show", hr);
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	IShellItem* item = 0;
	hr = dialog->GetResult(&item);
	if (FAILED(hr))
	{
		DialogLogHr("IFileSaveDialog::GetResult", hr);
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	PWSTR path = 0;
	hr = item->GetDisplayName(SIGDN_FILESYSPATH, &path);
	if (FAILED(hr))
	{
		DialogLogHr("IShellItem::GetDisplayName(save)", hr);
		item->Release();
		dialog->Release();
		if (need_uninit)
			CoUninitialize();
		return false;
	}

	xr_string result = ToAnsi(path);
	CoTaskMemFree(path);
	item->Release();
	dialog->Release();
	if (need_uninit)
		CoUninitialize();

	if (result.empty())
	{
		DialogLog("! IFileSaveDialog returned empty result");
		return false;
	}

	DialogLog("* IFileSaveDialog result=%s", result.c_str());
	return CopyResultToBuffer(result, buffer, sizeof(string_path));
}
//----------------------------------------------------
LPCSTR EFS_Utils::AppendFolderToName(LPSTR tex_name, u32 const tex_name_size, int depth, BOOL full_name)
{
	string256 _fn;
	xr_strcpy(tex_name,tex_name_size,AppendFolderToName(tex_name, _fn, sizeof(_fn), depth, full_name));
	return tex_name;
}

LPCSTR EFS_Utils::AppendFolderToName(LPCSTR src_name, LPSTR dest_name, u32 const dest_name_size, int depth, BOOL full_name)
{
	shared_str tmp = src_name;
    LPCSTR s 	= src_name;
    LPSTR d 	= dest_name;
    int sv_depth= depth;
	for (; *s&&depth; s++, d++){
		if (*s=='_'){depth--; *d='\\';}else{*d=*s;}
	}
	if (full_name){
		*d			= 0;
		if (depth<sv_depth)	xr_strcat(dest_name,dest_name_size,*tmp);
	}else{
		for (; *s; s++, d++) *d=*s;
		*d			= 0;
	}
    return dest_name;
}

LPCSTR EFS_Utils::GenerateName(LPCSTR base_path, LPCSTR base_name, LPCSTR def_ext, LPSTR out_name, u32 const out_name_size)
{
    int cnt = 0;
	string_path fn;
    if (base_name)	
		strconcat		(sizeof(fn), fn, base_path,base_name,def_ext);
	else 			
		xr_sprintf		(fn, sizeof(fn), "%s%02d%s",base_path,cnt++,def_ext);

	while (FS.exist(fn))
	    if (base_name)	
			xr_sprintf	(fn, sizeof(fn),"%s%s%02d%s",base_path,base_name,cnt++,def_ext);
        else 			
			xr_sprintf	(fn, sizeof(fn), "%s%02d%s",base_path,cnt++,def_ext);
    xr_strcpy(out_name,out_name_size,fn);
	return out_name;
}

//#endif
