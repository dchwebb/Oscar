CLEAR
m.samples = 128
m.FFTMode = .t.
m.output = ""

CREATE CURSOR curOutput (Cnt I, Cnd F(10, 6), Harmonic F(10, 6))
m.candFormula = "SIN((6.7 * 2 * PI() * m.i / m.samples)) + SIN((6.7 * 3 * 2 * PI() * m.i / m.samples))"


*	Square wave
*m.candFormula = "IIF(m.i < (m.samples / 2), 1, 0)"

*	Saw
m.candFormula = "(2 * (m.samples - m.i) / m.samples) - 1"

*	Sine
*m.candFormula = "SIN((2 * PI() * m.i / m.samples)) + 0.5 * SIN(5 * 2 * PI() * m.i / m.samples)"

*m.candFormula = "VAL(STREXTRACT(',0.3102, 0.694, .4731, 0.5149, 0.3393, 0.3336, 0.171, 0.1419, -0.0131, -0.0605, -0.2093, -0.2766, -0.4187, -0.5168, -0.6522, -0.8304,', ',', ',', m.i + 1))"

* samples from clipboard
*m.candFormula = "VAL(STREXTRACT(CHR(13) + _CLIPTEXT, CHR(13), CHR(13), m.i + 1))"

*	Create a table containing samples
CREATE CURSOR curSamples (Cnt I, Br I, Sin F(10, 6), Cos F(10, 6))

*	Alternate samples
FOR m.i = 0 TO m.samples - 1
	INSERT INTO curOutput (Cnt, Cnd) VALUES (m.i + 1, EVALUATE(m.candFormula))
	IF i % 2 = 0
		INSERT INTO curSamples (Cnt, Br, Sin) VALUES (m.i / 2, m.i / 2, EVALUATE(m.candFormula))
	ELSE
		SELECT curSamples
		REPLACE cos WITH EVALUATE(m.candFormula)
	ENDIF
ENDFOR

m.process = m.samples / 2

* Bit reverse samples
FOR m.i = 1 TO m.process
	m.br = 0
	FOR m.b = 1 TO LOG(m.process) / LOG(2)
		IF BITTEST(m.i, m.b - 1)
			m.br = BITSET(m.br, LOG(m.process) / LOG(2) - m.b)
		ENDIF
	ENDFOR
	
	*	Swap samples
	IF m.br > m.i
		SELECT curSamples
		LOCATE FOR cnt = m.i
		m.sinA = curSamples.sin
		LOCATE FOR cnt = m.br
		m.sinB = curSamples.sin
		REPLACE Br WITH m.i, sin WITH m.sinA
		LOCATE FOR cnt = m.i
		REPLACE Br WITH m.br, sin WITH m.sinB
	ENDIF
ENDFOR

GO TOP
BROWSE NORMAL NOWAIT

m.steps = 0

IF m.FFTMode

   n = samples
	mmax = 2
	DO WHILE n > mmax
		istep = mmax * 2
		theta = -(2 * PI() / mmax)
		wtemp = SIN(0.5 * theta)
		wpr = -2.0 * wtemp * wtemp
		wpi = SIN(theta)
		wr = 1.0
		wi = 0.0
		FOR m = 1 TO mmax - 1 STEP 2
			FOR i = m TO n STEP istep
				j = i + mmax
				
				SELECT curSamples
				LOCATE FOR cnt  = m.i - 1
				m.dim1 = curSamples.sin
				m.di = curSamples.cos

				LOCATE FOR cnt  = m.j - 1
				m.djm1 = curSamples.sin
				m.dj = curSamples.cos				
				
				tempr = wr * m.djm1 - wi * m.dj
				tempi = wr * m.dj + wi * m.djm1

				m.djm1 = m.dim1 - tempr
				m.dj = m.di - tempi
				m.dim1 = m.dim1 + tempr
				m.di = m.di + tempi
				
				SELECT curSamples
				REPLACE sin WITH m.djm1, cos WITH m.dj

				LOCATE FOR cnt  = m.i - 1				
				REPLACE sin WITH m.dim1, cos WITH m.di
				
			ENDFOR 

			wtemp = wr
			wr = wr + wr * wpr - wi * wpi
			wi = wi + wi * wpr + wtemp * wpi
		ENDFOR
		mmax = istep
	ENDDO    
    

	SELECT cnt, SQRT(sin ^ 2 + cos ^ 2) / (m.samples / 2) AS harmonic FROM cursamples WHERE cnt > 0 AND cnt <= m.samples / 2 INTO CURSOR curLengths
	SELECT curLengths
	SCAN
		SELECT curOutput
		REPLACE harmonic WITH curLengths.harmonic FOR cnt = curLengths.cnt
	ENDSCAN

ELSE

	*	Slow Fourier Transform
	FOR m.cs = 1 TO m.samples / 2

		m.cosSum = 0
		m.sinSum = 0

		FOR m.i = 0 TO m.samples - 1
		
			m.cand = EVALUATE(m.candFormula)
			
			m.cosine = COS(m.cs * 2 * PI() * m.i / m.samples)
			m.sine = SIN(m.cs * 2 * PI() * m.i / m.samples)

			m.cosSum = m.cosSum + (m.cosine * m.cand)
			m.sinSum = m.sinSum + (m.sine * m.cand)

			m.steps = m.steps + 1			
		ENDFOR

		m.Harmonic = SQRT((m.cosSum ^ 2) + (m.sinSum ^ 2)) / (m.samples / 2)


		SELECT curOutput
		LOCATE FOR cnt = m.cs
		REPLACE Harmonic WITH m.Harmonic
		
	ENDFOR

ENDIF

SELECT curOutput
COPY TO Output.xls TYPE XL5

ExcelExport(IIF(m.FFTMode, "FFT", "SFT") + "_Fourier")

GO TOP
*BROWSE NORMAL NOWAIT

WAIT WINDOW IIF(m.FFTMode, "FFT", "SFT") + ":   Samples: " + TRANSFORM(m.samples) + "   Steps: " + TRANSFORM(m.steps)



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