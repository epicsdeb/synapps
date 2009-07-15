Option Strict Off
Option Explicit On
Imports VB = Microsoft.VisualBasic
Friend Class roperServer
	Inherits System.Windows.Forms.Form
#Region "Windows Form Designer generated code "
	Public Sub New()
		MyBase.New()
		If m_vb6FormDefInstance Is Nothing Then
			If m_InitializingDefInstance Then
				m_vb6FormDefInstance = Me
			Else
				Try 
					'For the start-up form, the first instance created is the default instance.
					If System.Reflection.Assembly.GetExecutingAssembly.EntryPoint.DeclaringType Is Me.GetType Then
						m_vb6FormDefInstance = Me
					End If
				Catch
				End Try
			End If
		End If
		'This call is required by the Windows Form Designer.
		InitializeComponent()
	End Sub
	'Form overrides dispose to clean up the component list.
	Protected Overloads Overrides Sub Dispose(ByVal Disposing As Boolean)
		If Disposing Then
			If Not components Is Nothing Then
				components.Dispose()
			End If
		End If
		MyBase.Dispose(Disposing)
	End Sub
	'Required by the Windows Form Designer
	Private components As System.ComponentModel.IContainer
	Public ToolTip1 As System.Windows.Forms.ToolTip
	Public WithEvents StopServer As System.Windows.Forms.Button
	Public WithEvents StartServer As System.Windows.Forms.Button
	Public WithEvents Port As System.Windows.Forms.TextBox
	Public WithEvents CommandList As System.Windows.Forms.ListBox
	Public WithEvents Label1 As System.Windows.Forms.Label
	Public WithEvents Exit_Renamed As System.Windows.Forms.MenuItem
	Public WithEvents File As System.Windows.Forms.MenuItem
	Public MainMenu1 As System.Windows.Forms.MainMenu
	'NOTE: The following procedure is required by the Windows Form Designer
	'It can be modified using the Windows Form Designer.
	'Do not modify it using the code editor.
	<System.Diagnostics.DebuggerStepThrough()> Private Sub InitializeComponent()
		Dim resources As System.Resources.ResourceManager = New System.Resources.ResourceManager(GetType(roperServer))
		Me.components = New System.ComponentModel.Container()
		Me.ToolTip1 = New System.Windows.Forms.ToolTip(components)
		Me.ToolTip1.Active = True
		Me.StopServer = New System.Windows.Forms.Button
		Me.StartServer = New System.Windows.Forms.Button
		Me.Port = New System.Windows.Forms.TextBox
		Me.CommandList = New System.Windows.Forms.ListBox
		Me.Label1 = New System.Windows.Forms.Label
		Me.MainMenu1 = New System.Windows.Forms.MainMenu
		Me.File = New System.Windows.Forms.MenuItem
		Me.Exit_Renamed = New System.Windows.Forms.MenuItem
		Me.Text = "Roper Server"
		Me.ClientSize = New System.Drawing.Size(470, 262)
		Me.Location = New System.Drawing.Point(11, 52)
		Me.StartPosition = System.Windows.Forms.FormStartPosition.WindowsDefaultLocation
		Me.Font = New System.Drawing.Font("Arial", 8!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
		Me.AutoScaleBaseSize = New System.Drawing.Size(5, 13)
		Me.BackColor = System.Drawing.SystemColors.Control
		Me.FormBorderStyle = System.Windows.Forms.FormBorderStyle.Sizable
		Me.ControlBox = True
		Me.Enabled = True
		Me.KeyPreview = False
		Me.MaximizeBox = True
		Me.MinimizeBox = True
		Me.Cursor = System.Windows.Forms.Cursors.Default
		Me.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.ShowInTaskbar = True
		Me.HelpButton = False
		Me.WindowState = System.Windows.Forms.FormWindowState.Normal
		Me.Name = "roperServer"
		Me.StopServer.TextAlign = System.Drawing.ContentAlignment.MiddleCenter
		Me.StopServer.Text = "Stop Server"
		Me.StopServer.Size = New System.Drawing.Size(129, 25)
		Me.StopServer.Location = New System.Drawing.Point(296, 8)
		Me.StopServer.TabIndex = 4
		Me.StopServer.Font = New System.Drawing.Font("Arial", 8!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
		Me.StopServer.BackColor = System.Drawing.SystemColors.Control
		Me.StopServer.CausesValidation = True
		Me.StopServer.Enabled = True
		Me.StopServer.ForeColor = System.Drawing.SystemColors.ControlText
		Me.StopServer.Cursor = System.Windows.Forms.Cursors.Default
		Me.StopServer.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.StopServer.TabStop = True
		Me.StopServer.Name = "StopServer"
		Me.StartServer.TextAlign = System.Drawing.ContentAlignment.MiddleCenter
		Me.StartServer.Text = "Start Server"
		Me.StartServer.Size = New System.Drawing.Size(129, 25)
		Me.StartServer.Location = New System.Drawing.Point(160, 8)
		Me.StartServer.TabIndex = 0
		Me.StartServer.Font = New System.Drawing.Font("Arial", 8!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
		Me.StartServer.BackColor = System.Drawing.SystemColors.Control
		Me.StartServer.CausesValidation = True
		Me.StartServer.Enabled = True
		Me.StartServer.ForeColor = System.Drawing.SystemColors.ControlText
		Me.StartServer.Cursor = System.Windows.Forms.Cursors.Default
		Me.StartServer.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.StartServer.TabStop = True
		Me.StartServer.Name = "StartServer"
		Me.Port.AutoSize = False
		Me.Port.Size = New System.Drawing.Size(41, 25)
		Me.Port.Location = New System.Drawing.Point(104, 8)
		Me.Port.TabIndex = 1
		Me.Port.Text = "5001"
		Me.Port.Font = New System.Drawing.Font("Arial", 8!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
		Me.Port.AcceptsReturn = True
		Me.Port.TextAlign = System.Windows.Forms.HorizontalAlignment.Left
		Me.Port.BackColor = System.Drawing.SystemColors.Window
		Me.Port.CausesValidation = True
		Me.Port.Enabled = True
		Me.Port.ForeColor = System.Drawing.SystemColors.WindowText
		Me.Port.HideSelection = True
		Me.Port.ReadOnly = False
		Me.Port.Maxlength = 0
		Me.Port.Cursor = System.Windows.Forms.Cursors.IBeam
		Me.Port.MultiLine = False
		Me.Port.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.Port.ScrollBars = System.Windows.Forms.ScrollBars.None
		Me.Port.TabStop = True
		Me.Port.Visible = True
		Me.Port.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D
		Me.Port.Name = "Port"
		Me.CommandList.Size = New System.Drawing.Size(449, 202)
		Me.CommandList.Location = New System.Drawing.Point(8, 48)
		Me.CommandList.TabIndex = 3
		Me.CommandList.BorderStyle = System.Windows.Forms.BorderStyle.Fixed3D
		Me.CommandList.BackColor = System.Drawing.SystemColors.Window
		Me.CommandList.CausesValidation = True
		Me.CommandList.Enabled = True
		Me.CommandList.ForeColor = System.Drawing.SystemColors.WindowText
		Me.CommandList.IntegralHeight = True
		Me.CommandList.Cursor = System.Windows.Forms.Cursors.Default
		Me.CommandList.SelectionMode = System.Windows.Forms.SelectionMode.One
		Me.CommandList.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.CommandList.Sorted = False
		Me.CommandList.TabStop = True
		Me.CommandList.Visible = True
		Me.CommandList.MultiColumn = False
		Me.CommandList.Name = "CommandList"
		Me.Label1.Text = "Port to listen on:"
		Me.Label1.Size = New System.Drawing.Size(81, 25)
		Me.Label1.Location = New System.Drawing.Point(8, 8)
		Me.Label1.TabIndex = 2
		Me.Label1.Font = New System.Drawing.Font("Arial", 8!, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, CType(0, Byte))
		Me.Label1.TextAlign = System.Drawing.ContentAlignment.TopLeft
		Me.Label1.BackColor = System.Drawing.SystemColors.Control
		Me.Label1.Enabled = True
		Me.Label1.ForeColor = System.Drawing.SystemColors.ControlText
		Me.Label1.Cursor = System.Windows.Forms.Cursors.Default
		Me.Label1.RightToLeft = System.Windows.Forms.RightToLeft.No
		Me.Label1.UseMnemonic = True
		Me.Label1.Visible = True
		Me.Label1.AutoSize = False
		Me.Label1.BorderStyle = System.Windows.Forms.BorderStyle.None
		Me.Label1.Name = "Label1"
		Me.File.Text = "File"
		Me.File.Checked = False
		Me.File.Enabled = True
		Me.File.Visible = True
		Me.File.MDIList = False
		Me.Exit_Renamed.Text = "Exit"
		Me.Exit_Renamed.Checked = False
		Me.Exit_Renamed.Enabled = True
		Me.Exit_Renamed.Visible = True
		Me.Exit_Renamed.MDIList = False
		Me.Controls.Add(StopServer)
		Me.Controls.Add(StartServer)
		Me.Controls.Add(Port)
		Me.Controls.Add(CommandList)
		Me.Controls.Add(Label1)
		Me.File.Index = 0
		MainMenu1.MenuItems.AddRange(New System.Windows.Forms.MenuItem(){Me.File})
		Me.Exit_Renamed.Index = 0
		File.MenuItems.AddRange(New System.Windows.Forms.MenuItem(){Me.Exit_Renamed})
		Me.Menu = MainMenu1
	End Sub
#End Region 
#Region "Upgrade Support "
	Private Shared m_vb6FormDefInstance As roperServer
	Private Shared m_InitializingDefInstance As Boolean
	Public Shared Property DefInstance() As roperServer
		Get
			If m_vb6FormDefInstance Is Nothing OrElse m_vb6FormDefInstance.IsDisposed Then
				m_InitializingDefInstance = True
				m_vb6FormDefInstance = New roperServer()
				m_InitializingDefInstance = False
			End If
			DefInstance = m_vb6FormDefInstance
		End Get
		Set
			m_vb6FormDefInstance = Value
		End Set
	End Property
#End Region 
	
	' This is a Visual Basic server that listens for commands over a socket connection
	' and executes calls the Roper routines to drive WinView/WinSpec
	' All commands return a response, either the data from the command or OK if no data returned.
	
	' Mark Rivers
	' October 9, 2003
	
	Private Const MIN_ROI_SIZE As Short = 3
	Private Const INFINITE_EXPOSURE As Single = 1000#
	
	Private ListWidth As Short
	Private ListHeight As Short
	
	Dim WithEvents m_server As JBSOCKETSERVERLib.Server
	
	Dim ChipX As Short
	Dim ChipY As Short
	
	Dim CommentParams(5) As Integer
	Dim expSetup As WINX32Lib.ExpSetup
	Dim docFile As WINX32Lib.DocFile
	
    Public Sub Exit_Popup(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles Exit_Renamed.Popup
        Exit_Click(eventSender, eventArgs)
    End Sub
    Public Sub Exit_Click(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles Exit_Renamed.Click
        End
    End Sub

    Private Sub roperServer_Load(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles MyBase.Load
        ListWidth = VB6.PixelsToTwipsX(Me.Width) - VB6.PixelsToTwipsX(CommandList.Width)
        ListHeight = VB6.PixelsToTwipsY(Me.Height) - VB6.PixelsToTwipsY(CommandList.Height)
        StartServer.Enabled = True
        StopServer.Enabled = False
        ' Create the expSetup object - this will start WinView if it is not already running
        expSetup = New WINX32Lib.ExpSetup
        ChipX = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_XDIMDET)
        ChipY = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_YDIMDET)
        ' Set docfile to the current docFile document if it exists
        docFile = expSetup.GetDocument()
        CommentParams(1) = WINX32Lib.DM_CMD.DM_USERCOMMENT1
        CommentParams(2) = WINX32Lib.DM_CMD.DM_USERCOMMENT2
        CommentParams(3) = WINX32Lib.DM_CMD.DM_USERCOMMENT3
        CommentParams(4) = WINX32Lib.DM_CMD.DM_USERCOMMENT4
        CommentParams(5) = WINX32Lib.DM_CMD.DM_USERCOMMENT5
        StartServer_Click(StartServer, New System.EventArgs)
    End Sub

    Private Sub roperServer_Resize(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles MyBase.Resize
        Dim newHeight As Short

        newHeight = VB6.PixelsToTwipsY(Me.Height) - ListHeight
        If (newHeight < 100) Then
            newHeight = 100
        End If
        CommandList.SetBounds(CommandList.Left, CommandList.Top, VB6.TwipsToPixelsX(VB6.PixelsToTwipsX(Me.Width) - ListWidth), VB6.TwipsToPixelsY(newHeight))
    End Sub

    Private Sub StartServer_Click(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles StartServer.Click
        m_server = JBSOCKETSERVERLibSocketServerFactory_definst.CreateSocketServer(CInt(Port.Text))
        AddToList("Server started on port " & Port.Text)
        StartServer.Enabled = False
        StopServer.Enabled = True
        m_server.StartListening()
    End Sub

    Private Sub StopServer_Click(ByVal eventSender As System.Object, ByVal eventArgs As System.EventArgs) Handles StopServer.Click
        AddToList("Server stopped on port " & Port.Text)
        StartServer.Enabled = True
        StopServer.Enabled = False
        m_server.StopListening()
        m_server = Nothing
    End Sub

    Private Sub m_server_OnConnectionClose(ByVal Socket As JBSOCKETSERVERLib.Socket) Handles m_server.OnConnectionClosed
        AddToList("Connection closed : " & GetAddressAsString(Socket))
        Socket.UserData = 0
    End Sub

    Private Sub m_server_OnConnectionEstablished(ByVal Socket As JBSOCKETSERVERLib.Socket) Handles m_server.OnConnectionEstablished
        AddToList("Connection established : " & GetAddressAsString(Socket))
        Socket.RequestRead()
    End Sub

    Private Sub m_server_OnDataReceived(ByVal Socket As JBSOCKETSERVERLib.Socket, ByVal Data As JBSOCKETSERVERLib.data) Handles m_server.OnDataReceived

        Dim InputBuffer() As Byte
        Dim Input_String As String
        Dim Command_String As String
        Dim Params(5) As String
        Dim Response As String
        Dim i As Integer
        Dim Stat As Integer
        Dim BStat As Boolean
        Dim Busy As Integer
        Dim ROITop As Integer
        Dim ROIBottom As Integer
        Dim ROILeft As Integer
        Dim ROIRight As Integer
        Dim Xbin As Integer
        Dim Ybin As Integer
        Dim Temperature As Single
        Dim Exposure As Single
        Dim ROI As New WINX32Lib.ROIRect
        Dim TotalCounts As Double
        Dim NetCounts As Double
        Dim Sequents As Short
        Dim Controller As Short

        'InputString = Data.ReadString
        ' There seems to be a bug with ReadString, it crashes VB if the string is longer
        ' than 14 characters.  Use Read() instead until the problem is understood/fixed
        InputBuffer = Data.Read
        ParseInput(InputBuffer, Input_String, Command_String, Params)
        AddToList("Received : " & GetAddressAsString(Socket) & " - " & Input_String)

        Response = "OK"

        Select Case Command_String
            Case "shutter"
                If (CInt(Params(0)) = 1) Then
                    ' Stop current exposure, if any
                    expSetup.Stop()
                    Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_EXPOSURE, INFINITE_EXPOSURE)
                    ' Turn on detector
                    docFile = Nothing
                    Stat = expSetup.Start(docFile)
                Else
                    expSetup.Stop()
                End If

            Case "set_roi"
                ROITop = CInt(Params(0))
                If (ROITop < 1) Then ROITop = 1
                ROIBottom = CInt(Params(1))
                If (ROIBottom < (ROITop + MIN_ROI_SIZE)) Then ROIBottom = ROITop + MIN_ROI_SIZE
                If (ROIBottom > ChipY) Then ROIBottom = ChipY
                ROILeft = CInt(Params(2))
                If (ROILeft < 1) Then ROILeft = 1
                ROIRight = CInt(Params(3))
                If (ROIRight < (ROILeft + MIN_ROI_SIZE)) Then ROIRight = ROILeft + MIN_ROI_SIZE
                If (ROIRight > ChipX) Then ROIRight = ChipX
                ROI = expSetup.GetROI(1)
                If (TypeName(ROI) = "Nothing") Then
                    AddToList("No such ROI")
                Else
                    ROI.top = ROITop
                    ROI.left = ROILeft
                    ROI.bottom = ROIBottom
                    ROI.right = ROIRight
                    expSetup.ClearROIs()
                    expSetup.SetROI(ROI)
                    Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_USEROI, 1)
                End If

            Case "get_roi"
                ROI = expSetup.GetROI(1)
                If (TypeName(ROI) = "Nothing") Then
                    AddToList("No such ROI")
                Else
                    Response = CStr(ROI.top) & "," & CStr(ROI.bottom) & "," & CStr(ROI.left) & "," & CStr(ROI.right)
                End If

            Case "set_binning"
                Xbin = CInt(Params(0))
                Ybin = CInt(Params(1))
                If (Xbin < 1) Then Xbin = 1
                If (Ybin < 1) Then Ybin = 1
                ROI = expSetup.GetROI(1)
                If (TypeName(ROI) = "Nothing") Then
                    AddToList("No such ROI")
                Else
                    ROI.XGroup = Xbin
                    ROI.YGroup = Ybin
                    expSetup.ClearROIs()
                    expSetup.SetROI(ROI)
                    Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_USEROI, 1)
                End If

            Case "get_binning"
                ROI = expSetup.GetROI(1)
                If (TypeName(ROI) = "Nothing") Then
                    AddToList("No such ROI")
                End If
                Response = CStr(ROI.XGroup) & "," & CStr(ROI.YGroup)

            Case "set_nframes"
                Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_SEQUENTS, CShort(Params(0)))

            Case "get_nframes"
                Sequents = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_SEQUENTS)
                Response = CStr(Sequents)

            Case "set_exposure"
                Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_EXPOSURE, CSng(Params(0)))

            Case "get_exposure"
                Exposure = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_EXPOSURE)
                Response = CStr(Exposure)

            Case "set_temperature"
                Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_TEMPERATURE, CSng(Params(0)))

            Case "get_temperature"
                Temperature = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_ACTUAL_TEMP)
                Response = CStr(Temperature)

            Case "start"
                ' Stop current exposure, if any
                expSetup.Stop()
                ' Turn on detector
                If (docFileValid()) Then
                    docFile.Close()
                    docFile = Nothing
                End If
                BStat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_BBACKSUBTRACT, Params(0))
                BStat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_BDOFLATFIELD, Params(1))
                BStat = expSetup.Start(docFile)
                If (BStat = False) Then Response = "Error"

            Case "acquire_background"
                ' Stop current exposure, if any
                Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_DARKNAME, Params(0))
                expSetup.Stop()
                ' Turn on detector
                BStat = expSetup.AcquireBackground()
                If (BStat = False) Then Response = "Error"

            Case "acquire_flatfield"
                Stat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_FLATFLDNAME, Params(0))
                ' Stop current exposure, if any
                expSetup.Stop()
                BStat = expSetup.AcquireFlatfield()
                If (BStat = False) Then Response = "Error"

            Case "get_state"
                Busy = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_RUNNING)
                Response = CStr(Busy)

            Case "get_roi_counts"
                ComputeCounts(TotalCounts, NetCounts)
                Response = CStr(TotalCounts) & "," & CStr(NetCounts)

            Case "set_comment"
                i = CInt(Params(0))
                If (docFileValid()) Then
                    Stat = docFile.SetParam(CommentParams(i), Params(1))
                End If

            Case "get_comment"
                i = CInt(Params(0))
                If (docFileValid()) Then
                    Response = docFile.GetParam(CommentParams(i))
                End If

            Case "get_controller"
                Controller = expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_CONTROLLER_NAME)
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
                    Stat = docFile.SetParam(WINX32Lib.DM_CMD.DM_FILENAME, Params(0))
                    Stat = docFile.Save()
                End If

                ' The following 4 commands are designed to be user-extensible to support operations
                ' not built in to the command set
            Case "set_exp_param"
                Select Case Params(0)
                    Case "EXP_TIMING_MODE"
                        BStat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_TIMING_MODE, CInt(Params(1)))
                    Case "EXP_SHUTTER_CONTROL"
                        BStat = expSetup.SetParam(WINX32Lib.EXP_CMD.EXP_SHUTTER_CONTROL, CInt(Params(1)))
                    Case Else
                        Response = "Unknown EXP param " & Params(0)
                End Select

            Case "get_exp_param"
                Select Case Params(0)
                    Case "EXP_TIMING_MODE"
                        Response = CStr(expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_TIMING_MODE))
                    Case "EXP_SHUTTER_CONTROL"
                        Response = CStr(expSetup.GetParam(WINX32Lib.EXP_CMD.EXP_SHUTTER_CONTROL))
                    Case Else
                        Response = "Unknown EXP param " & Params(0)
                End Select

            Case "set_doc_param"
                If (docFileValid()) Then
                    Select Case Params(0)
                        Case "DM_XLABEL"
                            BStat = docFile.SetParam(WINX32Lib.DM_CMD.DM_XLABEL, Params(1))
                        Case "DM_YLABEL"
                            BStat = docFile.SetParam(WINX32Lib.DM_CMD.DM_YLABEL, Params(1))
                        Case Else
                            Response = "Unknown DOC param " & Params(0)
                    End Select
                Else
                    Response = "No docFile"
                End If

            Case "get_doc_param"
                If (docFileValid()) Then
                    Select Case Params(0)
                        Case "DM_XLABEL"
                            Response = docFile.GetParam(WINX32Lib.DM_CMD.DM_XLABEL)
                        Case "DM_YLABEL"
                            Response = docFile.GetParam(WINX32Lib.DM_CMD.DM_YLABEL)
                        Case "DM_FILEDATE"
                            Response = docFile.GetParam(WINX32Lib.DM_CMD.DM_FILEDATE)
                        Case "DM_FILEVERSION"
                            Response = docFile.GetParam(WINX32Lib.DM_CMD.DM_FILEVERSION)
                        Case Else
                            Response = "Unknown DOC param " & Params(0)
                    End Select
                Else
                    Response = "No docFile"
                End If

            Case Else
                Response = "Invalid command"

        End Select

        Socket.WriteString(Response & Chr(13) & Chr(10))
        AddToList("Sent     : " & GetAddressAsString(Socket) & " - " & Response)
        Socket.RequestRead()

    End Sub

    Private Sub ParseInput(ByRef InputBuffer() As Byte, ByRef Input_String As String, ByRef Command_String As String, ByRef Params() As String)
        Dim First As Short
        Dim Last As Short
        Dim i As Short
        Dim InputLen As Short

        InputLen = UBound(InputBuffer)
        Input_String = ""
        For i = 0 To InputLen
            Input_String = Input_String & Chr(InputBuffer(i))
        Next i
        ' Get rid of CR, LF in input string
        Input_String = Replace(Input_String, Chr(10), "")
        Input_String = Replace(Input_String, Chr(13), "")
        ' Parse the input string
        First = 1
        InputLen = Len(Input_String)
        Last = InStr(First, Input_String, ",") - 1
        If (Last < 0) Then Last = InputLen
        Command_String = VB.Left(Input_String, Last)
        ' Get rid of blanks in the command
        Command_String = Replace(Command_String, " ", "")
        For i = 0 To UBound(Params)
            First = Last + 2
            Last = InStr(First, Input_String, ",") - 1
            If (Last < 0) Then Last = InputLen
            If (First <= InputLen) Then
                Params(i) = Mid(Input_String, First, Last - First + 1)
            Else
                Params(i) = ""
            End If
        Next i
    End Sub

    Private Sub AddToList(ByRef message As String)
        If CommandList.Items.Count = 20000 Then
            CommandList.Items.Clear()
        End If
        CommandList.Items.Add(message)
        CommandList.SelectedIndex = CommandList.Items.Count - 1
    End Sub

    Private Function GetAddressAsString(ByRef Socket As JBSOCKETSERVERLib.ISocket) As String
        GetAddressAsString = Socket.RemoteAddress.Address & " : " & Socket.RemoteAddress.Port
    End Function

    Private Sub ComputeCounts(ByRef total As Double, ByRef net As Double)
        Dim buff As Object
        Dim i, j As Integer
        Dim Stat As Integer
        Dim sum, back As Double
        Dim ly, ux, lx, nx, uy As Integer

        ' Read data from camera
        If (docFileValid()) Then
            docFile.GetFrame(1, buff)
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
                back = (IntegerToUnsigned(buff(lx, j)) + _
                        IntegerToUnsigned(buff(lx + 1, j)) + _
                        IntegerToUnsigned(buff(ux - 1, j)) + _
                        IntegerToUnsigned(buff(ux, j))) / 4
                sum = 0
                For i = lx To ux
                    sum = sum + IntegerToUnsigned(buff(i, j))
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

    Function IntegerToUnsigned(ByVal Value As Integer) As Long
        If Value < 0 Then
            IntegerToUnsigned = Value + 65536
        Else
            IntegerToUnsigned = Value
        End If
    End Function
End Class