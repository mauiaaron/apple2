REM Script can be loaded from apple2ix from debugger console with "LOAD path/to/thisfile"
REM
REM Tests program execution across the main/aux memory RAMRD boundary (C003 enables aux, C002 enables main)
REM     * 1E00 is the test script that will place a $96 result at 1FFF (in main)
REM     * 1E80 calls AUXMOVE (C311) to load the 1E00 page to auxmem
REM

CALL-151
!
1E00: NOP
 STA $C003
 LDA #$96
 STA $1FFF
 STA $C002
 RTS
1E80: LDA #$00
 STA $3C
 LDA #$1E
 STA $3D
 LDA #$FF
 STA $3E
 LDA #$1E
 STA $3F
 LDA #$00
 STA $42
 LDA #$1E
 STA $43
 SEC
 JSR $C311
 RTS

1E80G
1E00G
