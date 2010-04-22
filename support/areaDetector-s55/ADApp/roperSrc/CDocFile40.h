// Machine generated IDispatch wrapper class(es) created with Add Class from Typelib Wizard

#import "C:\\Program Files\\PI Acton\\WinView\\Winview.exe" no_namespace
// CDocFile40 wrapper class

class CDocFile40 : public COleDispatchDriver
{
public:
	CDocFile40(){} // Calls COleDispatchDriver default constructor
	CDocFile40(LPDISPATCH pDispatch) : COleDispatchDriver(pDispatch) {}
	CDocFile40(const CDocFile40& dispatchSrc) : COleDispatchDriver(dispatchSrc) {}

	// Attributes
public:

	// Operations
public:


	// IDocFile4 methods
public:
	void GetFramePointer(short frame, VARIANT * pFrame)
	{
		static BYTE parms[] = VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0x1, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, pFrame);
	}
	void GetStripPointer(short strip, short frame, VARIANT * pFrame)
	{
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0x2, DISPATCH_METHOD, VT_EMPTY, NULL, parms, strip, frame, pFrame);
	}
	VARIANT GetParam(long Param, short * pbRes)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I4 VTS_PI2 ;
		InvokeHelper(0x3, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, Param, pbRes);
		return result;
	}
	short SetParam(long Param, VARIANT * pSetVal)
	{
		short result;
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x4, DISPATCH_METHOD, VT_I2, (void*)&result, parms, Param, pSetVal);
		return result;
	}
	BOOL Open(LPCTSTR Name, VARIANT& xLen, VARIANT& yLen, VARIANT& zLen, VARIANT& dataType, VARIANT& newName)
	{
		BOOL result;
		static BYTE parms[] = VTS_BSTR VTS_VARIANT VTS_VARIANT VTS_VARIANT VTS_VARIANT VTS_VARIANT ;
		InvokeHelper(0x5, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, Name, &xLen, &yLen, &zLen, &dataType, &newName);
		return result;
	}
	LPDISPATCH GetWindow()
	{
		LPDISPATCH result;
		InvokeHelper(0x6, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, NULL);
		return result;
	}
	void Update()
	{
		InvokeHelper(0x7, DISPATCH_METHOD, VT_EMPTY, NULL, NULL);
	}
	VARIANT get_bufPtr()
	{
		VARIANT result;
		InvokeHelper(0x8, DISPATCH_PROPERTYGET, VT_VARIANT, (void*)&result, NULL);
		return result;
	}
	void AllocFrame(VARIANT * FrameVariant)
	{
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0x9, DISPATCH_METHOD, VT_EMPTY, NULL, parms, FrameVariant);
	}
	void GetFrame(short frame, VARIANT * FrameVariant)
	{
		static BYTE parms[] = VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0xa, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, FrameVariant);
	}
	void PutFrame(short frame, VARIANT * FrameArray)
	{
		static BYTE parms[] = VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0xb, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, FrameArray);
	}
	void GetStrip(short frame, short strip, VARIANT * StripVariant)
	{
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0xc, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, StripVariant);
	}
	void PutStrip(short frame, short strip, VARIANT& StripVariant)
	{
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_VARIANT ;
		InvokeHelper(0xd, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, &StripVariant);
	}
	VARIANT GetPixel(short frame, short strip, short pixel)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_I2 ;
		InvokeHelper(0xe, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, frame, strip, pixel);
		return result;
	}
	void PutPixel(short frame, short strip, short pixel, VARIANT& thePixel)
	{
		static BYTE parms[] = VTS_I2 VTS_I2 VTS_I2 VTS_VARIANT ;
		InvokeHelper(0xf, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, pixel, &thePixel);
	}
	void FreePointer(VARIANT * pFrame)
	{
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0x10, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pFrame);
	}
	short Save()
	{
		short result;
		InvokeHelper(0x11, DISPATCH_METHOD, VT_I2, (void*)&result, NULL);
		return result;
	}
	short Close()
	{
		short result;
		InvokeHelper(0x12, DISPATCH_METHOD, VT_I2, (void*)&result, NULL);
		return result;
	}
	void SetDoc(long * pDoc)
	{
		static BYTE parms[] = VTS_PI4 ;
		InvokeHelper(0x13, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pDoc);
	}
	LPDISPATCH GetCalibration()
	{
		LPDISPATCH result;
		InvokeHelper(0x14, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, NULL);
		return result;
	}
	void SetCalibration(LPDISPATCH pCalib)
	{
		static BYTE parms[] = VTS_DISPATCH ;
		InvokeHelper(0x15, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pCalib);
	}
	BOOL OpenNew(LPCTSTR Name, LPDISPATCH pInfo)
	{
		BOOL result;
		static BYTE parms[] = VTS_BSTR VTS_DISPATCH ;
		InvokeHelper(0x16, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, Name, pInfo);
		return result;
	}
	BOOL SaveAs(LPCTSTR Name, long nType)
	{
		BOOL result;
		static BYTE parms[] = VTS_BSTR VTS_I4 ;
		InvokeHelper(0x17, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, Name, nType);
		return result;
	}
	LPDISPATCH GetWindow2(short WindowNum)
	{
		LPDISPATCH result;
		static BYTE parms[] = VTS_I2 ;
		InvokeHelper(0x18, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, parms, WindowNum);
		return result;
	}
	void AddFrame(short frame, VARIANT& FrameArray)
	{
		static BYTE parms[] = VTS_I2 VTS_VARIANT ;
		InvokeHelper(0x19, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, &FrameArray);
	}
	CString get_Title()
	{
		CString result;
		InvokeHelper(0x1a, DISPATCH_PROPERTYGET, VT_BSTR, (void*)&result, NULL);
		return result;
	}
	void put_Title(LPCTSTR newValue)
	{
		static BYTE parms[] = VTS_BSTR ;
		InvokeHelper(0x1a, DISPATCH_PROPERTYPUT, VT_EMPTY, NULL, parms, newValue);
	}
	VARIANT SGetParam(long Param, VARIANT * pbRes)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x1b, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, Param, pbRes);
		return result;
	}
	void GetFrame2(long frame, VARIANT * FrameVariant)
	{
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x1c, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, FrameVariant);
	}
	void PutFrame2(long frame, VARIANT * FrameArray)
	{
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x1d, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, FrameArray);
	}
	void GetStrip2(long frame, short strip, VARIANT * StripVariant)
	{
		static BYTE parms[] = VTS_I4 VTS_I2 VTS_PVARIANT ;
		InvokeHelper(0x1e, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, StripVariant);
	}
	void PutStrip2(long frame, short strip, VARIANT& StripVariant)
	{
		static BYTE parms[] = VTS_I4 VTS_I2 VTS_VARIANT ;
		InvokeHelper(0x1f, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, &StripVariant);
	}
	VARIANT GetPixel2(long frame, short strip, short pixel)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I4 VTS_I2 VTS_I2 ;
		InvokeHelper(0x20, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, frame, strip, pixel);
		return result;
	}
	void PutPixel2(long frame, short strip, short pixel, VARIANT& thePixel)
	{
		static BYTE parms[] = VTS_I4 VTS_I2 VTS_I2 VTS_VARIANT ;
		InvokeHelper(0x21, DISPATCH_METHOD, VT_EMPTY, NULL, parms, frame, strip, pixel, &thePixel);
	}

	// IDocFile4 properties
public:

};
