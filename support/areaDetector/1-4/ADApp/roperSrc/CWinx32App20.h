// Machine generated IDispatch wrapper class(es) created with Add Class from Typelib Wizard

#import "C:\\Program Files\\PI Acton\\WinView\\Winview.exe" no_namespace
// CWinx32App20 wrapper class

class CWinx32App20 : public COleDispatchDriver
{
public:
	CWinx32App20(){} // Calls COleDispatchDriver default constructor
	CWinx32App20(LPDISPATCH pDispatch) : COleDispatchDriver(pDispatch) {}
	CWinx32App20(const CWinx32App20& dispatchSrc) : COleDispatchDriver(dispatchSrc) {}

	// Attributes
public:

	// Operations
public:


	// IWinx32App2 methods
public:
	void ShowDemoBox(LPCTSTR pText)
	{
		static BYTE parms[] = VTS_BSTR ;
		InvokeHelper(0x1, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pText);
	}
	long GetAppWnd()
	{
		long result;
		InvokeHelper(0x2, DISPATCH_METHOD, VT_I4, (void*)&result, NULL);
		return result;
	}
	BOOL GetAppRectangle(VARIANT * pRect)
	{
		BOOL result;
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0x3, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, pRect);
		return result;
	}
	BOOL GetAppRect(long * top, long * left, long * bottom, long * right)
	{
		BOOL result;
		static BYTE parms[] = VTS_PI4 VTS_PI4 VTS_PI4 VTS_PI4 ;
		InvokeHelper(0x4, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, top, left, bottom, right);
		return result;
	}
	long CountOpenDocs()
	{
		long result;
		InvokeHelper(0x5, DISPATCH_METHOD, VT_I4, (void*)&result, NULL);
		return result;
	}
	BOOL CloseOpenDocs(short * pcDocs)
	{
		BOOL result;
		static BYTE parms[] = VTS_PI2 ;
		InvokeHelper(0x6, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, pcDocs);
		return result;
	}
	BOOL InfoBox(BOOL bOn)
	{
		BOOL result;
		static BYTE parms[] = VTS_BOOL ;
		InvokeHelper(0x7, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, bOn);
		return result;
	}
	BOOL StatusBar(BOOL bOn)
	{
		BOOL result;
		static BYTE parms[] = VTS_BOOL ;
		InvokeHelper(0x8, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, bOn);
		return result;
	}
	long get_rectTop()
	{
		long result;
		InvokeHelper(0x9, DISPATCH_PROPERTYGET, VT_I4, (void*)&result, NULL);
		return result;
	}
	long get_rectLeft()
	{
		long result;
		InvokeHelper(0xa, DISPATCH_PROPERTYGET, VT_I4, (void*)&result, NULL);
		return result;
	}
	long get_rectBottom()
	{
		long result;
		InvokeHelper(0xb, DISPATCH_PROPERTYGET, VT_I4, (void*)&result, NULL);
		return result;
	}
	long get_rectRight()
	{
		long result;
		InvokeHelper(0xc, DISPATCH_PROPERTYGET, VT_I4, (void*)&result, NULL);
		return result;
	}
	CString get_Version()
	{
		CString result;
		InvokeHelper(0xd, DISPATCH_PROPERTYGET, VT_BSTR, (void*)&result, NULL);
		return result;
	}
	CString get_Flavor()
	{
		CString result;
		InvokeHelper(0xe, DISPATCH_PROPERTYGET, VT_BSTR, (void*)&result, NULL);
		return result;
	}
	void Hide(BOOL bHide)
	{
		static BYTE parms[] = VTS_BOOL ;
		InvokeHelper(0xf, DISPATCH_METHOD, VT_EMPTY, NULL, parms, bHide);
	}
	BOOL StatusBarMsg(short index, short color, LPCTSTR Text)
	{
		BOOL result;
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_BSTR ;
		InvokeHelper(0x10, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, index, color, Text);
		return result;
	}
	void ShowProgress(short Percent)
	{
		static BYTE parms[] = VTS_I2 ;
		InvokeHelper(0x11, DISPATCH_METHOD, VT_EMPTY, NULL, parms, Percent);
	}
	BOOL Quit()
	{
		BOOL result;
		InvokeHelper(0x12, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	BOOL get_Visible()
	{
		BOOL result;
		InvokeHelper(0x13, DISPATCH_PROPERTYGET, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	void put_Visible(BOOL newValue)
	{
		static BYTE parms[] = VTS_BOOL ;
		InvokeHelper(0x13, DISPATCH_PROPERTYPUT, VT_EMPTY, NULL, parms, newValue);
	}
	CString ShowFileOpen(LPCTSTR InName)
	{
		CString result;
		static BYTE parms[] = VTS_BSTR ;
		InvokeHelper(0x14, DISPATCH_METHOD, VT_BSTR, (void*)&result, parms, InName);
		return result;
	}
	CString ShowFileSaveAs(LPCTSTR InName)
	{
		CString result;
		static BYTE parms[] = VTS_BSTR ;
		InvokeHelper(0x15, DISPATCH_METHOD, VT_BSTR, (void*)&result, parms, InName);
		return result;
	}
	void SetNotify(LPUNKNOWN pNotifier)
	{
		static BYTE parms[] = VTS_UNKNOWN ;
		InvokeHelper(0x16, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pNotifier);
	}
	void SaveAll()
	{
		InvokeHelper(0x17, DISPATCH_METHOD, VT_EMPTY, NULL, NULL);
	}
	void LoadFactoryDefaults(LPCTSTR INIFile)
	{
		static BYTE parms[] = VTS_BSTR ;
		InvokeHelper(0x18, DISPATCH_METHOD, VT_EMPTY, NULL, parms, INIFile);
	}
	BOOL SGetAppRect(VARIANT * top, VARIANT * left, VARIANT * bottom, VARIANT * right)
	{
		BOOL result;
		static BYTE parms[] = VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT VTS_PVARIANT ;
		InvokeHelper(0x19, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, top, left, bottom, right);
		return result;
	}
	BOOL SCloseOpenDocs(VARIANT * pcDocs)
	{
		BOOL result;
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0x1a, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, pcDocs);
		return result;
	}

	// IWinx32App2 properties
public:

};
