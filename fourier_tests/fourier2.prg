CLEAR
m.s = 128
m.output = ""

CREATE CURSOR curOutput (Cnt I, Cnd F(10, 6), Harmonic F(10, 6))


*	Square wave
m.candFormula = "IIF(m.i < (m.s / 2), 1, 0)"

*	Saw
m.candFormula = "(2 * (m.s - m.i) / m.s) - 1"

m.candFormula = "SIN((7.3 * 2 * PI() * m.i / m.s)) + SIN((7.3 * 3 * 2 * PI() * m.i / m.s))"


m.output = m.output + "cand" + CHR(9)
FOR m.i = 0 TO m.s - 1
	INSERT INTO curOutput (Cnt, Cnd) VALUES (m.i + 1, EVALUATE(m.candFormula))

*	m.output = m.output + TRANSFORM(EVALUATE(m.candFormula), "99.999") + CHR(9)
ENDFOR

m.output = m.output + "prd" + CHR(13)

FOR m.cs = 1 TO m.s / 2

	m.cosSum = 0
	m.sinSum = 0

	FOR m.i = 0 TO m.s - 1
	
		m.cand = EVALUATE(m.candFormula)
		
		m.cosine = COS(m.cs * 2 * PI() * m.i / m.s)
		m.sine = SIN(m.cs * 2 * PI() * m.i / m.s)

		m.cosSum = m.cosSum + (m.cosine * m.cand)
		m.sinSum = m.sinSum + (m.sine * m.cand)
		
	ENDFOR

	m.Harmonic = SQRT((m.cosSum ^ 2) + (m.sinSum ^ 2)) / (m.s / 2)


	SELECT curOutput
	LOCATE FOR cnt = m.cs
	REPLACE Harmonic WITH m.Harmonic
ENDFOR

SELECT curOutput
COPY TO D:\docs\ARM\Oscar\fourier_tests\Output.xls TYPE XL5

ExcelExport("Fourier")

RETURN



*********************
PROCEDURE ExcelExport
LPARAMETERS m.tmpName, m.tmpTitle, m.tmpOptions, m.tmpNotCurrency
*	Copies a cursor to an Excel spreadsheet
*	m.tmpOptions BIT - 1 = Do not format numerics as currency, 2 = format currencies with 2dp, 4 = append datetime to name if cannot create, 8 = create only
*	m.tmpNotCurrency - list of fields to exclude from currency formatting
LOCAL m.x, m.tmpFields, m.tmpAlias

m.TempPath = ADDBS(SYS(2023))

m.tmpOptions = EVL(m.tmpOptions, 0)
m.tmpName = TRIM(EVL(m.tmpName, ALIAS()))
m.tmpSave = m.tmpName
m.tmpNotCurrency = EVL(m.tmpNotCurrency, "")
m.tmpAlias = ALIAS()

m.XlError = .F.
ON ERROR m.XlError = .T.

IF RECCOUNT() > 65535
	COPY TO (m.TempPath + m.tmpName + ".csv") TYPE CSV
ELSE
	COPY TO (m.TempPath + m.tmpName + ".xls") TYPE XL5
ENDIF

m.ErrCount = 0
DO WHILE m.XlError AND m.ErrCount < 5 		&&AND MESSAGEBOX("Cannot create spreadsheet. Close previous spreadsheet and try again?", 4, "Error Capture") = 6
	m.ErrCount = m.ErrCount + 1
	m.XlError = .F.
	m.tmpAltName = m.tmpName + "_" + TTOC(DATETIME(), 1)
	IF RECCOUNT() > 65535
		COPY TO (m.TempPath + m.tmpAltName + ".csv") TYPE CSV
	ELSE
		COPY TO (m.TempPath + m.tmpAltName + ".xls") TYPE XL5
	ENDIF
	IF !m.XlError
		m.tmpName = m.tmpAltName
	ENDIF
ENDDO
IF m.XlError
	MESSAGEBOX("Cannot create spreadsheet")
	RETURN .F.
ENDIF

IF !RunExcel()
	RETURN .F.
ENDIF

objExcel.Visible = .T.
objExcel.ScreenUpdating = .T.
ON ERROR

PUBLIC oWB, oWS
oWB = objExcel.Workbooks.Open(m.TempPath + m.tmpName + IIF(RECCOUNT() > 65535, ".csv", ".xls"))
oWS = oWB.ActiveSheet
ExcelFormat(oWS)

IF BITTEST(m.tmpOptions, 4-1)
	RETURN
ENDIF


oChartWS = oWB.Sheets.Add(oWB.Sheets(1))
oChartWS.Name = "Chart"

oChart = oChartWS.Shapes.AddChart2(227, 4)		&& xlLine = 4
*m.tmpRange = "A1:" + CHR(64 + FCOUNT()) + TRANSFORM(RECCOUNT() + 1)
m.tmpRange = "B1:B" + TRANSFORM(RECCOUNT() + 1)

oChart.Chart.SetSourceData(oWS.Range(m.tmpRange))		&& A - M are number of columns representing number of regions

oChart.Top = 10
oChart.Left = 10
oChart.Width = 700
oChart.Height = 220

oChart2 = oChartWS.Shapes.AddChart2(227, 51)		&& xlColumnClustered = 51
m.tmpRange = "C2:C" + TRANSFORM(RECCOUNT() / 2 + 1)

oChart2.Chart.SetSourceData(oWS.Range(m.tmpRange))		&& A - M are number of columns representing number of regions

oChart2.Top = 250
oChart2.Left = 10
oChart2.Width = 700
oChart2.Height = 220



IF !EMPTY(m.tmpTitle)
	oChart.Chart.ChartTitle.Text = m.tmpTitle
ELSE
	oChart.Chart.SetElement(0)		&& no title
ENDIF

oChart.Chart.SetElement(101)	&& Legend right

oWB.Saved = .T.

RETURN .T.


*********************
PROCEDURE ExcelFormat
LPARAMETERS tmpWs

*	Tidy up headers
FOR m.x = 1 TO FCOUNT(ALIAS())
	tmpWs.Cells(1, m.x).Value = PROPER(STRTRAN(tmpWs.Cells(1, m.x).Value, "_", " "))
ENDFOR
tmpWs.Rows("1:1").Font.Bold = .T.		&& bold header
*tmpWs.Rows("1:1").AutoFilter
objExcel.ActiveWindow.SplitRow = 1
objExcel.ActiveWindow.FreezePanes = .T.

*	Autosize up to 26 columns
oWS.Columns("A:AG").EntireColumn.AutoFit

*	Replace blank dates with blank
m.tmpAlerts = objExcel.Application.DisplayAlerts
objExcel.Application.DisplayAlerts = .F.

tmpWs.Cells.Replace("  -   -", "")
tmpWs.Columns("A:G").EntireColumn.AutoFit


objExcel.Application.DisplayAlerts = m.tmpAlerts


RETURN

******************
PROCEDURE RunExcel
*	Starts the word processor, returning .t. if created or .f. if not

WAIT WINDOW NOWAIT "Starting excel ..."

IF TYPE("objExcel") != "O" OR TYPE("objExcel.Name") != "C"
	PUBLIC objExcel
	
	m.XlError = .F.
	ON ERROR m.XlError = .T.

	objExcel = CreateObject("Excel.Application")
	ON ERROR
	IF m.XlError
		MESSAGEBOX("Error Starting Excel")
		RETURN .F.
	ENDIF
ENDIF
RETURN .T.