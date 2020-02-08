CLEAR
m.s = 16
m.output = ""

m.candFormula = "SIN((2 * PI() * m.i / m.s) + 0.5) + COS((4 * 2 * PI() * m.i / m.s)) "

m.output = m.output + "cand" + CHR(9)
FOR m.i = 0 TO m.s - 1
	m.output = m.output + TRANSFORM(EVALUATE(m.candFormula), "99.999") + CHR(9)
ENDFOR

m.output = m.output + "prd" + CHR(13)

FOR m.cs = 0 TO m.s

	IF MOD(m.cs, 2) = 0
		m.output = m.output + "COS" + TRANSFORM(FLOOR(m.cs / 2)) + CHR(9)
	ELSE
		m.output = m.output + "SIN" + TRANSFORM(FLOOR(m.cs / 2)) + CHR(9)
	ENDIF

	m.sumProduct = 0

	FOR m.i = 0 TO m.s - 1
	
		m.cand = EVALUATE(m.candFormula)
		
		IF MOD(m.cs, 2) = 0
			m.test = COS(FLOOR(m.cs / 2) * 2 * PI() * m.i / m.s)
			m.output = m.output + TRANSFORM(m.test * m.cand, "99.999") + CHR(9)
			
		ELSE
			m.test = SIN(FLOOR(m.cs / 2) * 2 * PI() * m.i / m.s)
			m.output = m.output + TRANSFORM(m.test * m.cand, "99.999") + CHR(9)
		ENDIF

		m.sumProduct = m.sumProduct + (m.test * m.cand)
	ENDFOR

	m.output = m.output + TRANSFORM(m.sumProduct / (m.s / 2), "99.999") + CHR(13)	
ENDFOR

_CLIPTEXT = m.output

 