// Windows/Control/ComboBox.h

#ifndef __WINDOWS_CONTROL_COMBOBOX_H
#define __WINDOWS_CONTROL_COMBOBOX_H

#include "../../Common/MyWindows.h"

#include <commctrl.h>

#include "../Window.h"

namespace NWindows {
namespace NControl {

class CComboBox: public CWindow
{
public:
  void ResetContent() { SendMsg(CB_RESETCONTENT, 0, 0); }
  LRESULT AddString(LPCTSTR s) { return SendMsg(CB_ADDSTRING, 0, (LPARAM)s); }
  #ifndef _UNICODE
  LRESULT AddString(LPCWSTR s);
  #endif
  LRESULT SetCurSel(int index) { return SendMsg(CB_SETCURSEL, index, 0); }
  int GetCurSel() { return (int)SendMsg(CB_GETCURSEL, 0, 0); }
  int GetCount() { return (int)SendMsg(CB_GETCOUNT, 0, 0); }
  
  LRESULT GetLBTextLen(int index) { return SendMsg(CB_GETLBTEXTLEN, index, 0); }
  LRESULT GetLBText(int index, LPTSTR s) { return SendMsg(CB_GETLBTEXT, index, (LPARAM)s); }
  LRESULT GetLBText(int index, CSysString &s);
  #ifndef _UNICODE
  LRESULT GetLBText(int index, UString &s);
  #endif

  LRESULT SetItemData(int index, LPARAM lParam) { return SendMsg(CB_SETITEMDATA, index, lParam); }
  LRESULT GetItemData(int index) { return SendMsg(CB_GETITEMDATA, index, 0); }

  LRESULT GetItemData_of_CurSel() { return GetItemData(GetCurSel()); }

  void ShowDropDown(bool show = true) { SendMsg(CB_SHOWDROPDOWN, show ? TRUE : FALSE, 0);  }
};

#ifndef UNDER_CE

class CComboBoxEx: public CComboBox
{
public:
  bool SetUnicodeFormat(bool fUnicode) { return LRESULTToBool(SendMsg(CBEM_SETUNICODEFORMAT, BOOLToBool(fUnicode), 0)); }

  LRESULT DeleteItem(int index) { return SendMsg(CBEM_DELETEITEM, index, 0); }
  LRESULT InsertItem(COMBOBOXEXITEM *item) { return SendMsg(CBEM_INSERTITEM, 0, (LPARAM)item); }
  #ifndef _UNICODE
  LRESULT InsertItem(COMBOBOXEXITEMW *item) { return SendMsg(CBEM_INSERTITEMW, 0, (LPARAM)item); }
  #endif

  LRESULT SetItem(COMBOBOXEXITEM *item) { return SendMsg(CBEM_SETITEM, 0, (LPARAM)item); }
  DWORD SetExtendedStyle(DWORD exMask, DWORD exStyle) { return (DWORD)SendMsg(CBEM_SETEXTENDEDSTYLE, exMask, exStyle); }
  HWND GetEditControl() { return (HWND)SendMsg(CBEM_GETEDITCONTROL, 0, 0); }
  HIMAGELIST SetImageList(HIMAGELIST imageList) { return (HIMAGELIST)SendMsg(CBEM_SETIMAGELIST, 0, (LPARAM)imageList); }
};

#endif

}}

#endif
