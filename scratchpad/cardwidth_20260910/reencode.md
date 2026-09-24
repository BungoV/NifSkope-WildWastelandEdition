# Lane CARDWIDTH -- the re-encoding, on the sheet bytes

## N. { a' >= 128 } == { a >= 16 }, in PNG and after a BC3 round trip

| tree | texels a>=16 | a'>=128 | disagreeing | after BC3: a'>=128 | disagreeing | worst halfW move, texels |
|---|---|---|---|---|---|---|
| 0003a28b | 209793 | 209793 | 0 | 204543 | 5250 | 0.00 |
| 0004a074 | 64709 | 64709 | 0 | 62614 | 2095 | 0.50 |
| 00038599 | 13510 | 13510 | 0 | 13481 | 29 | 0.00 |

## O. The fraction is recoverable: decode(encode(a)) against a

| tree | texels a>=16 | max |decode-a| | mean |decode-a| |
|---|---|---|---|
| 0003a28b | 209793 | 1 | 0.393 |
| 0004a074 | 64709 | 1 | 0.424 |
| 00038599 | 13510 | 1 | 0.253 |

## P. Floor: a re-encoding with the floor at 64 must NOT reproduce the 16/255 set

| tree | floor 16: disagreeing texels | floor 64: disagreeing | floor is discriminating? |
|---|---|---|---|
| 0003a28b | 0 | 50663 | YES |
| 0004a074 | 0 | 18967 | YES |
| 00038599 | 0 | 1357 | YES |

