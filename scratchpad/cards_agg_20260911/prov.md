
### Provenance of §4.6 (lane CARDS-AGG, 2026-09-11)

Source hashes taken FIRST, every line number below found again from its own
anchor text (each matched exactly once), the version constants re-read last
(`ww-contract-provenance`).

| file | sha256 (16) | lines |
|---|---|---|
| `src/lodifile.h` | `78f94b0162a68a1e` | 447 |
| `src/lodifile.cpp` | `996d02e8f7ea8214` | 1,492 |
| `src/lodgenaggregate.h` | `b24833822dad3567` | 159 |
| `src/lodgenaggregate.cpp` | `e31dd7b97fac4c4d` | 690 |
| `src/nativeemit.cpp` | `63d5f8a87ba6c73b` | 1,323 |

| claim | line | anchor |
|---|---|---|
| the aggregate row is 48 bytes | `lodifile.h:130` | `constexpr quint16 LODI_AGGREGATE_STRIDE = 48;` |
| the row's own layout | `lodifile.h:264` | `struct LodiAggregate` |
| the switch threshold's default, 96 reference pixels | `lodifile.h:139` | `constexpr float LODI_AGG_SWITCH_PX = 96.0f;` |
| the aggregate identity space is the top bit | `lodifile.h:143` | `constexpr quint32 LODI_AGG_IDENTITY_BIT = 0x80000000U;` |
| one identity per aggregate, and it is the row's index | `lodifile.cpp:512` | `row.identity = LODI_AGG_IDENTITY_BIT \| quint32( ai );` |
| **the version is CONDITIONAL** (Deviation 12) | `lodifile.cpp:555` | `h.version = aggs.empty() ? LODI_VERSION : LODI_VERSION_AGGREGATE;` |
| the two new payloads join `indexCrc32`, after the occluders | `lodifile.cpp:591` | `h.indexCrc32 = lodvCrc32( reinterpret_cast<const unsigned char *>( aggs.data() )` |
| the header pad starts at 0xD4 on a v4 file and 0xB0 on a v3 one | `lodifile.cpp:764` | `const int padFrom = ( h.version == LODI_VERSION_AGGREGATE ) ? H_RESERVED_D4 : H_RESERVED_B0;` |
| the view basis: right = up x eye | `lodgenaggregate.cpp:285` | `right[v][0] = -eye[v][1]; right[v][1] = eye[v][0]; right[v][2] = 0.0f;` |
| the gap law, unchanged from the card sheets | `lodgenaggregate.cpp:42` | `int gapOf( int side )` |
| the area weight (§10.4 of the card sheets) | `lodgenaggregate.cpp:469` | `const float wgt = texelArea > 0.0f ? sampleArea / texelArea : 1.0f;` |
| the height channel re-encoded through the aggregate's own span | `lodgenaggregate.cpp:585` | `const float behind = -( D.depth * inv );` |
| the `.lodm` says the identity law in words | `lodgenaggregate.cpp:667` | `a.insert( QStringLiteral( "identity" ), QStringLiteral( "per-aggregate" ) );` |
