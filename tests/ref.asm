FUNC LABEL 0
    MOV STK A VAL 5
    REF STK B STK A
    PRINT PTR B
    REF REG 0 STK A
    MOV STK C REG 0
    PRINT PTR C
    REF STK C STK B
    REF PTR C STK A
    PRINT PTR B
    RET
