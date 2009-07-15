VERSION 5.00
Begin VB.Form roperServer 
   Caption         =   "Roper Server"
   ClientHeight    =   3930
   ClientLeft      =   165
   ClientTop       =   780
   ClientWidth     =   7050
   LinkTopic       =   "roperServer"
   ScaleHeight     =   3930
   ScaleWidth      =   7050
   StartUpPosition =   3  'Windows Default
   Begin VB.CommandButton StopServer 
      Caption         =   "Stop Server"
      Height          =   375
      Left            =   4440
      TabIndex        =   4
      Top             =   120
      Width           =   1935
   End
   Begin VB.CommandButton StartServer 
      Caption         =   "Start Server"
      Height          =   375
      Left            =   2400
      TabIndex        =   0
      Top             =   120
      Width           =   1935
   End
   Begin VB.TextBox Port 
      Height          =   375
      Left            =   1560
      TabIndex        =   1
      Text            =   "5001"
      Top             =   120
      Width           =   612
   End
   Begin VB.ListBox CommandList 
      BeginProperty Font 
         Name            =   "Fixedsys"
         Size            =   9
         Charset         =   0
         Weight          =   400
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   2985
      Left            =   120
      TabIndex        =   3
      Top             =   720
      Width           =   6735
   End
   Begin VB.Label Label1 
      Caption         =   "Port to listen on:"
      Height          =   375
      Left            =   120
      TabIndex        =   2
      Top             =   120
      Width           =   1215
   End
   Begin VB.Menu File 
      Caption         =   "File"
      Begin VB.Menu Exit 
         Caption         =   "Exit"
      End
   End
End
Attribute VB_Name = "roperServer"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Option Explicit

' This is a Visual Basic server that listens for commands over a socket connection
' and executes calls the Roper routines to drive WinView/WinSpec
' All commands return a response, either the data from the command or OK if no data returned.

' Mark Rivers
' October 9, 2003

Private Const MIN_ROI_SIZE As Integer = 3
Private Const INFINITE_EXPOSURE As Single = 1000#

Private ListWidth As Integer
Private ListHeight As Integer

Dim WithEvents m_server As JBSOCKETSERVERLib.Server
Attribute m_server.VB_VarHelpID = -1

Dim ChipX As Integer
Dim ChipY As Integer

Dim CommentParams(5) As Long
Dim expSetup As winx32lib.expSetup
Dim docFile As winx32lib.docFile

Private Sub Exit_Click()
    End
End Sub

Private Sub Form_Load()
    ListWidth = Me.Width - CommandList.Width
    ListHeight = Me.Height - CommandList.Height
    StartServer.Enabled = True
    StopServer.Enabled = False
    ' Create the expSetup object - this will start WinView if it is not already running
    Set expSetup = New winx32lib.expSetup
    ChipX = expSetup.GetParam(EXP_XDIMDET)
    ChipY = expSetup.GetParam(EXP_YDIMDET)
    ' Set docfile to the current docFile document if it exists
    Set docFile = expSetup.GetDocument()
    CommentParams(1) = DM_USERCOMMENT1
    CommentParams(2) = DM_USERCOMMENT2
    CommentParams(3) = DM_USERCOMMENT3
    CommentParams(4) = DM_USERCOMMENT4
    CommentParams(5) = DM_USERCOMMENT5
    StartServer_Click
End Sub

Private Sub Form_Resize()
    Dim newHeight As Integer
    
    newHeight = Me.Height - ListHeight
    If (newHeight < 100) Then
        newHeight = 100
    End If
    CommandList.Move CommandList.Left, CommandList.Top, Me.Width - ListWidth, newHeight
End Sub

Private Sub StartServer_Click()
    Set m_server = CreateSocketServer(CLng(Port.Text))
    AddToList "Server started on port " & Port.Text
    StartServer.Enabled = False
    StopServer.Enabled = True
    m_server.StartListening
End Sub

Private Sub StopServer_Click()
    AddToList "Server stopped on port " & Port.Text
    StartServer.Enabled = True
    StopServer.Enabled = False
    Set m_server = Nothing
End Sub

Private Sub m_server_OnConnectionClosed(ByVal Socket As JBSOCKETSERVERLib.ISocket)
    AddToList "Connection closed : " & GetAddressAsString(Socket)
    Socket.UserData = 0
End Sub

Private Sub m_server_OnConnectionEstablished(ByVal Socket As JBSOCKETSERVERLib.ISocket)
    AddToList "Connection established : " & GetAddressAsString(Socket)
    Socket.RequestRead
End Sub

Private Sub m_server_OnDataReceived( _
    ByVal Socket As JBSOCKETSERVERLib.ISocket, _
    ByVal Data As JBSOCKETSERVERLib.IData)

    Dim InputBuffer() As Byte
    Dim InputString As String
    Dim Command As String
    Dim Params(5) As String
    Dim Response As String
    Dim i As Long
    Dim Stat As Long
    Dim BStat As Boolean
    Dim Busy As Long
    Dim ROITop As Long
    Dim ROIBottom As Long
    Dim ROILeft As Long
    Dim ROIRight As Long
    Dim Xbin As Long
    Dim Ybin As Long
    Dim Temperature As Single
    Dim Exposure As Single
    Dim ROI As New ROIRect
    Dim TotalCounts As Double
    Dim NetCounts As Double
    Dim Sequents As Integer
    Dim Controller As Integer

    'InputString = Data.ReadString
    ' There seems to be a bug with ReadString, it crashes VB if the string is longer
    ' than 14 characters.  Use Read() instead until the problem is understood/fixed
    InputBuffer = Data.Read
    ParseInput InputBuffer, InputString, Command, Params
    AddToList "Received : " & GetAddressAsString(Socket) & " - " & InputString
    
    Response = "OK"
    
    Select Case Command
    Case "shutter"
       If (CLng(Params(0)) = 1) Then
            ' Stop current exposure, if any
            expSetup.Stop
            Stat = expSetup.SetParam(EXP_EXPOSURE, INFINITE_EXPOSURE)
            ' Turn on detector
            Set docFile = Nothing
            Stat = expSetup.Start(docFile)
        Else
            expSetup.Stop
        End If
    
    Case "set_roi"
        ROITop = CLng(Params(0))
        If (ROITop < 1) Then ROITop = 1
        ROIBottom = CLng(Params(1))
        If (ROIBottom < (ROITop + MIN_ROI_SIZE)) Then ROIBottom = ROITop + MIN_ROI_SIZE
        If (ROIBottom > ChipY) Then ROIBottom = ChipY
        ROILeft = CLng(Params(2))
        If (ROILeft < 1) Then ROILeft = 1
        ROIRight = CLng(Params(3))
        If (ROIRight < (ROILeft + MIN_ROI_SIZE)) Then ROIRight = ROILeft + MIN_ROI_SIZE
        If (ROIRight > ChipX) Then ROIRight = ChipX
        Set ROI = expSetup.GetROI(1)
        If (TypeName(ROI) = "Nothing") Then
            AddToList "No such ROI"
        End If
        ROI.Top = ROITop
        ROI.Left = ROILeft
        ROI.bottom = ROIBottom
        ROI.Right = ROIRight
        expSetup.ClearROIs
        expSetup.SetROI ROI
        Stat = expSetup.SetParam(EXP_USEROI, 1)
    
    Case "get_roi"
        Set ROI = expSetup.GetROI(1)
        If (TypeName(ROI) = "Nothing") Then
            AddToList "No such ROI"
        End If
        Response = CStr(ROI.Top) & "," & _
                   CStr(ROI.bottom) & "," & _
                   CStr(ROI.Left) & "," & _
                   CStr(ROI.Right)
    
    Case "set_binning"
        Xbin = CLng(Params(0))
        Ybin = CLng(Params(1))
        If (Xbin < 1) Then Xbin = 1
        If (Ybin < 1) Then Ybin = 1
        Set ROI = expSetup.GetROI(1)
        If (TypeName(ROI) = "Nothing") Then
            AddToList "No such ROI"
        End If
        ROI.XGroup = Xbin
        ROI.YGroup = Ybin
        expSetup.ClearROIs
        expSetup.SetROI ROI
        Stat = expSetup.SetParam(EXP_USEROI, 1)

    Case "get_binning"
        Set ROI = expSetup.GetROI(1)
        If (TypeName(ROI) = "Nothing") Then
            AddToList "No such ROI"
        End If
        Response = CStr(ROI.XGroup) & "," & CStr(ROI.YGroup)
        
    Case "set_nframes"
        Stat = expSetup.SetParam(EXP_SEQUENTS, CInt(Params(0)))
    
    Case "get_nframes"
        Sequents = expSetup.GetParam(EXP_SEQUENTS)
        Response = CStr(Sequents)
    
    Case "set_exposure"
        Stat = expSetup.SetParam(EXP_EXPOSURE, CSng(Params(0)))
    
    Case "get_exposure"
        Exposure = expSetup.GetParam(EXP_EXPOSURE)
        Response = CStr(Exposure)
    
    Case "set_temperature"
        Stat = expSetup.SetParam(EXP_TEMPERATURE, CSng(Params(0)))
    
    Case "get_temperature"
        Temperature = expSetup.GetParam(EXP_ACTUAL_TEMP)
        Response = CStr(Temperature)
        
    Case "start"
        ' Stop current exposure, if any
        expSetup.Stop
        ' Turn on detector
        If (docFileValid()) Then
            docFile.Close
            Set docFile = Nothing
        End If
        BStat = expSetup.SetParam(EXP_BBACKSUBTRACT, Params(0))
        BStat = expSetup.SetParam(EXP_BDOFLATFIELD, Params(1))
        BStat = expSetup.Start(docFile)
        If (BStat = False) Then Response = "Error"
        
    Case "acquire_background"
        ' Stop current exposure, if any
        Stat = expSetup.SetParam(EXP_DARKNAME, Params(0))
        expSetup.Stop
        ' Turn on detector
        BStat = expSetup.AcquireBackground()
        If (BStat = False) Then Response = "Error"
        
    Case "acquire_flatfield"
        Stat = expSetup.SetParam(EXP_FLATFLDNAME, Params(0))
        ' Stop current exposure, if any
        expSetup.Stop
        BStat = expSetup.AcquireFlatfield()
        If (BStat = False) Then Response = "Error"
        
    Case "get_state"
        Busy = expSetup.GetParam(EXP_RUNNING)
        Response = CStr(Busy)
       
    Case "get_roi_counts"
        ComputeCounts TotalCounts, NetCounts
        Response = CStr(TotalCounts) & "," & CStr(NetCounts)

    Case "set_comment"
        i = CLng(Params(0))
        If (docFileValid()) Then
            Stat = docFile.SetParam(CommentParams(i), Params(1))
        End If
 
    Case "get_comment"
        i = CLng(Params(0))
        If (docFileValid()) Then
            Response = docFile.GetParam(CommentParams(i))
        End If

    Case "get_controller"
        Controller = expSetup.GetParam(EXP_CONTROLLER_NAME)
        ' These are the only controllers I know the names of because of printing error in
        ' WinX Automation manual
        Select Case Controller
        Case 0
            Response = "No Controller"
        Case 1
            Response = "ST143"
        Case 2
            Response = "ST130"
        Case 3
            Response = "ST138"
        Case 4
            Response = "VICCD BOX"
        Case 5
            Response = "PentaMax"
        Case 6
            Response = "ST120_T1"
        Case 7
            Response = "ST120_T2"
        Case 8
            Response = "ST121"
        Case 9
            Response = "ST135"
        Case 10
            Response = "ST133"
        Case 11
            Response = "VICCD"
        Case 12
            Response = "ST116"
        Case 13
            Response = "OMA3"
        Case 14
            Response = "LOW_COST_SPEC"
        Case 15
            Response = "MICROMAX"
        Case 16
            Response = "SPECTROMAX"
        Case 17
            Response = "MICROVIEW"
        Case 18
            Response = "ST133_5MHZ"
        Case 19
            Response = "EMPTY_5MHZ"
        Case 20
            Response = "EPIX_CONTROLLER"
        Case 21
            Response = "PVCAM"
        Case 22
            Response = "GENERIC"
        Case 23
            Response = "ARC_CCD_100"
        Case 24
            Response = "ST133_2MHZ"
        Case Else
            Response = "Unknown"
        End Select
            
    Case "save"
        If (docFileValid()) Then
            Stat = docFile.SetParam(DM_FILENAME, Params(0))
            Stat = docFile.Save()
        End If
        
    ' The following 4 commands are designed to be user-extensible to support operations
    ' not built in to the command set
    Case "set_exp_param"
        Select Case Params(0)
        Case "EXP_TIMING_MODE"
            BStat = expSetup.SetParam(EXP_TIMING_MODE, CLng(Params(1)))
        Case "EXP_SHUTTER_CONTROL"
            BStat = expSetup.SetParam(EXP_SHUTTER_CONTROL, CLng(Params(1)))
        Case Else
            Response = "Unknown EXP param " & Params(0)
        End Select

    Case "get_exp_param"
        Select Case Params(0)
        Case "EXP_TIMING_MODE"
            Response = CStr(expSetup.GetParam(EXP_TIMING_MODE))
        Case "EXP_SHUTTER_CONTROL"
            Response = CStr(expSetup.GetParam(EXP_SHUTTER_CONTROL))
        Case Else
            Response = "Unknown EXP param " & Params(0)
        End Select

    Case "set_doc_param"
        If (docFileValid) Then
            Select Case Params(0)
            Case "DM_XLABEL"
                BStat = docFile.SetParam(DM_XLABEL, Params(1))
            Case "DM_YLABEL"
                BStat = docFile.SetParam(DM_YLABEL, Params(1))
            Case Else
                Response = "Unknown DOC param " & Params(0)
            End Select
         Else
            Response = "No docFile"
       End If

    Case "get_doc_param"
        If (docFileValid) Then
            Select Case Params(0)
            Case "DM_XLABEL"
                Response = docFile.GetParam(DM_XLABEL)
            Case "DM_YLABEL"
                Response = docFile.GetParam(DM_YLABEL)
            Case "DM_FILEDATE"
                Response = docFile.GetParam(DM_FILEDATE)
            Case "DM_FILEVERSION"
                Response = docFile.GetParam(DM_FILEVERSION)
            Case Else
                Response = "Unknown DOC param " & Params(0)
            End Select
        Else
            Response = "No docFile"
        End If

    Case Else
        Response = "Invalid command"
        
    End Select

    Socket.WriteString Response & Chr(13) & Chr(10)
    AddToList "Sent     : " & GetAddressAsString(Socket) & " - " & Response
    Socket.RequestRead
    
End Sub

Private Sub ParseInput(InputBuffer() As Byte, InputString As String, Command As String, Params() As String)
    Dim First As Integer
    Dim Last As Integer
    Dim i As Integer
    Dim InputLen As Integer
    
    InputLen = UBound(InputBuffer)
    InputString = ""
    For i = 0 To InputLen
       InputString = InputString & Chr(InputBuffer(i))
    Next i
    ' Get rid of CR, LF in input string
    InputString = Replace(InputString, Chr(10), "")
    InputString = Replace(InputString, Chr(13), "")
    ' Parse the input string
    First = 1
    InputLen = Len(InputString)
    Last = InStr(First, InputString, ",") - 1
    If (Last < 0) Then Last = InputLen
    Command = Left$(InputString, Last)
    ' Get rid of blanks in the command
    Command = Replace(Command, " ", "")
    For i = 0 To UBound(Params)
        First = Last + 2
        Last = InStr(First, InputString, ",") - 1
        If (Last < 0) Then Last = InputLen
        If (First <= InputLen) Then
            Params(i) = Mid(InputString, First, Last - First + 1)
        Else
            Params(i) = ""
        End If
    Next i
End Sub

Private Sub AddToList(message As String)
    If CommandList.ListCount = 20000 Then
        CommandList.Clear
    End If
    CommandList.AddItem message
    CommandList.ListIndex = CommandList.ListCount - 1
End Sub

Private Function GetAddressAsString(Socket As JBSOCKETSERVERLib.ISocket) As String
    GetAddressAsString = Socket.RemoteAddress.Address & " : " & Socket.RemoteAddress.Port
End Function

Private Sub ComputeCounts(total As Double, net As Double)
    Dim buff As Variant
    Dim i As Long, j As Long
    Dim Stat As Long
    Dim sum As Double, back As Double
    Dim lx As Long, ux As Long, nx As Long, ly As Long, uy As Long

    ' Read data from camera
    If (docFileValid()) Then
        docFile.GetFrame 1, buff
        ' Determine bounds of data array
        lx = LBound(buff, 1)
        ux = UBound(buff, 1)
        nx = ux - lx + 1
        ly = LBound(buff, 2)
        uy = UBound(buff, 2)
        total = 0
        net = 0
        For j = ly To uy
            ' Compute background - average of 2 points on end of each row
            back = (buff(lx, j) + buff(lx + 1, j) + buff(ux - 1, j) + buff(ux, j)) / 4
            sum = 0
            For i = lx To ux
                sum = sum + buff(i, j)
            Next i
            total = total + sum
            net = net + sum - (back * nx)
        Next j
    Else
        total = 0
        net = 0
    End If
End Sub

Private Function docFileValid() As Boolean
    docFileValid = Not (docFile Is Nothing)
End Function

