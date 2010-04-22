// Machine generated IDispatch wrapper class(es) created with Add Class from Typelib Wizard

#import "C:\\Program Files\\PI Acton\\WinView\\Winview.exe" no_namespace
// CExpSetup20 wrapper class

class CExpSetup20 : public COleDispatchDriver
{
public:
	CExpSetup20(){} // Calls COleDispatchDriver default constructor
	CExpSetup20(LPDISPATCH pDispatch) : COleDispatchDriver(pDispatch) {}
	CExpSetup20(const CExpSetup20& dispatchSrc) : COleDispatchDriver(dispatchSrc) {}

	// Attributes
public:

	// Operations
public:


	// IExpSetup2 methods
public:
	VARIANT GetParam(long Param, short * pRes)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I4 VTS_PI2 ;
		InvokeHelper(0x1, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, Param, pRes);
		return result;
	}
	short SetParam(long Param, VARIANT * pSetVal)
	{
		short result;
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x2, DISPATCH_METHOD, VT_I2, (void*)&result, parms, Param, pSetVal);
		return result;
	}
	LPDISPATCH GetDocument()
	{
		LPDISPATCH result;
		InvokeHelper(0x3, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, NULL);
		return result;
	}
	BOOL Start(LPDISPATCH * pFile)
	{
		BOOL result;
		static BYTE parms[] = VTS_PDISPATCH ;
		InvokeHelper(0x4, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, pFile);
		return result;
	}
	BOOL StartFocus(LPDISPATCH * pFile)
	{
		BOOL result;
		static BYTE parms[] = VTS_PDISPATCH ;
		InvokeHelper(0x5, DISPATCH_METHOD, VT_BOOL, (void*)&result, parms, pFile);
		return result;
	}
	short Stop()
	{
		short result;
		InvokeHelper(0x6, DISPATCH_METHOD, VT_I2, (void*)&result, NULL);
		return result;
	}
	short IsAvail(long Param, LPDISPATCH * pRange)
	{
		short result;
		static BYTE parms[] = VTS_I4 VTS_PDISPATCH ;
		InvokeHelper(0x7, DISPATCH_METHOD, VT_I2, (void*)&result, parms, Param, pRange);
		return result;
	}
	void SetROI(LPDISPATCH pROI)
	{
		static BYTE parms[] = VTS_DISPATCH ;
		InvokeHelper(0x8, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pROI);
	}
	LPDISPATCH GetROI(long index)
	{
		LPDISPATCH result;
		static BYTE parms[] = VTS_I4 ;
		InvokeHelper(0x9, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, parms, index);
		return result;
	}
	void ClearROIs()
	{
		InvokeHelper(0xa, DISPATCH_METHOD, VT_EMPTY, NULL, NULL);
	}
	void Load(BOOL * pRes)
	{
		static BYTE parms[] = VTS_PBOOL ;
		InvokeHelper(0xb, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pRes);
	}
	void Save(BOOL * pRes)
	{
		static BYTE parms[] = VTS_PBOOL ;
		InvokeHelper(0xc, DISPATCH_METHOD, VT_EMPTY, NULL, parms, pRes);
	}
	BOOL WaitForExperiment()
	{
		BOOL result;
		InvokeHelper(0xd, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	LPDISPATCH Start2(VARIANT * pRes)
	{
		LPDISPATCH result;
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0xe, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, parms, pRes);
		return result;
	}
	LPDISPATCH StartFocus2(VARIANT * pRes)
	{
		LPDISPATCH result;
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0xf, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, parms, pRes);
		return result;
	}
	BOOL AcquireBackground()
	{
		BOOL result;
		InvokeHelper(0x10, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	BOOL AcquireFlatfield()
	{
		BOOL result;
		InvokeHelper(0x11, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	VARIANT SGetParam(long Param, VARIANT * pRes)
	{
		VARIANT result;
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x12, DISPATCH_METHOD, VT_VARIANT, (void*)&result, parms, Param, pRes);
		return result;
	}
	LPDISPATCH IsAvail2(long Param, VARIANT * pRes)
	{
		LPDISPATCH result;
		static BYTE parms[] = VTS_I4 VTS_PVARIANT ;
		InvokeHelper(0x13, DISPATCH_METHOD, VT_DISPATCH, (void*)&result, parms, Param, pRes);
		return result;
	}
	BOOL StartNoGUI()
	{
		BOOL result;
		InvokeHelper(0x14, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	BOOL StopNoGUI()
	{
		BOOL result;
		InvokeHelper(0x15, DISPATCH_METHOD, VT_BOOL, (void*)&result, NULL);
		return result;
	}
	long GetFrameNoGUI(VARIANT * frame)
	{
		long result;
		static BYTE parms[] = VTS_PVARIANT ;
		InvokeHelper(0x16, DISPATCH_METHOD, VT_I4, (void*)&result, parms, frame);
		return result;
	}

	// IExpSetup2 properties
public:

};
