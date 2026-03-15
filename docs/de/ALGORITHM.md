\newpage

# Secasy — Algorithmus-Beschreibung

## Inhaltsverzeichnis

1. Motivation — Warum ein neuer Hash-Algorithmus?
2. Das Kernkonzept — Gitterbasierter Zustand
3. Datenstruktur im Detail
4. Phase 1 — Initialisierung
5. Phase 2 — Eingabe-Integration (Fingerprint-Bildung)
6. Phase 3 — Verarbeitungsrunden (Diffusion)
7. Phase 4 — Hash-Extraktion
8. Sicherheitsargumente
9. Vergleich mit bekannten Konstruktionen
10. Bekannte Grenzen und offene Fragen
11. Literatur

---

\newpage

## 1. Motivation — Warum ein neuer Hash-Algorithmus?

### Das dominante Paradigma: Merkle-Damgård

Die bekanntesten kryptographischen Hash-Funktionen — MD5, SHA-1, SHA-256,
SHA-512 — basieren alle auf der **Merkle-Damgård-Konstruktion** aus dem Jahr
1989 [1, 2]. Das Prinzip ist einfach: Die Eingabe wird in gleich große Blöcke
aufgeteilt, die sequenziell durch eine Kompressionsfunktion verarbeitet werden.
Der Ausgabe-Zustand nach dem letzten Block ist der Hash.

Diese lineare Verarbeitungskette hat eine bekannte strukturelle Schwäche:
den **Length Extension Attack**. Kennt ein Angreifer $H(m)$, kann er — ohne
$m$ zu kennen — $H(m \| \text{padding} \| m')$ für beliebige Fortsetzungen
$m'$ berechnen. Der Grund: Der Hash-Ausgabewert *ist* der interne Zustand
der Kompressionsfunktion; der Angreifer „setzt" die Berechnung einfach fort.
SHA-3 (Keccak) [4] und BLAKE2 [5] lösen dieses Problem durch andersartige Konstruktionen.

### Die Leitfrage hinter Secasy

*Was passiert, wenn man den internen Zustand radikal von der Ausgabe entkoppelt
und die Diffusion räumlich statt sequenziell organisiert?*

Secasy erkundet diesen Ansatz: Ein **zweidimensionales Gitter** als Zustandsraum,
in dem jedes Eingabebyte eine nichtlineare Bewegung durch das Gitter auslöst
und dabei Zellen verändert. Der interne Zustand (16.384 Bit) ist um den Faktor
32 größer als die Ausgabe (512 Bit) — was Length Extension strukturell
unmöglich macht und gleichzeitig die Bound gegen Kollisionsangriffe verbreitert.

---

## 2. Das Kernkonzept — Gitterbasierter Zustand

### Räumliche statt sequenzieller Diffusion

Klassische Hash-Funktionen bewegen Daten durch eine Kette von Blöcken:

```
Eingabe → [Block₁] → [Block₂] → [Block₃] → ... → Hash
```

Secasy hingegen bewegt einen **Cursor** durch ein zweidimensionales Feld:

```
           Spalte 0   1   2  ...  15
Zeile 0  [ ●   ·   ·  ...  · ]
Zeile 1  [ ·   ·   ·  ...  · ]
...      [                    ]
Zeile 15 [ ·   ·   ·  ...  · ]
              ↑
         256 Zellen, jede mit eigenem Zustand
```

Jedes Eingabebyte wird in vier 2-Bit-Richtungscodes zerlegt. Jeder Code
steuert eine Cursorbewegung und modifiziert die besuchte Zelle. Nach dem
letzten Eingabebyte trägt das Gitter einen **einzigartigen Fingerabdruck**
der Eingabe — dann erst beginnt die eigentliche Diffusionsphase.

### Warum ist das strukturell anders?

Bei Merkle-Damgård ist der Zustand nach Block $i$ vollständig durch die
Ausgabe von Block $i$ beschrieben — der Zustandsraum ist gleich groß wie
die Ausgabe. Bei Secasy ist der Zustandsraum $256 \times 64 = 16.384$ Bit
groß, die Ausgabe jedoch nur 512 Bit. Selbst mit vollständiger Kenntnis
des Hash-Wertes hat der Angreifer nur 3 % des internen Zustands — eine
Rekonstruktion des Feldes ist algebraisch nicht durchführbar.

---

## 3. Datenstruktur im Detail

Das Gitter besteht aus **256 Zellen** in einem 16×16-Raster. Jede Zelle
speichert drei unabhängige Werte:

| Feld         | Typ                                    | Bedeutung                                                                     |
|--------------|----------------------------------------|-------------------------------------------------------------------------------|
| `value`      | `int64_t` (64 Bit, vorzeichenbehaftet) | Numerischer Zellwert; wird durch Operationen verändert                        |
| `primeIndex` | `uint32_t`                             | Zeiger in die vorberechnete Primzahltabelle                                   |
| `colorIndex` | `uint8_t` (0–5)                        | Bestimmt, welche der 6 Operationen in Phase 3 auf diese Zelle angewendet wird |

Zusätzlich gibt es eine **Primzahltabelle** (`primes.h`) mit den ersten
16.000.000 Primzahlen (~355 KB). Diese Tabelle wird bei der Initialisierung
aus dem Header kompiliert und dient als deterministischer Zufallsgenerator
für Sprungweiten.

Der **Cursor** ist eine zweidimensionale Position $(x, y)$ im Bereich
$[0, 15] \times [0, 15]$, die sich während Phase 2 durch das Gitter bewegt.

---

## 4. Phase 1 — Initialisierung

Die Initialisierung ist **eingabe-unabhängig** und vollständig deterministisch.
Jede Ausführung beginnt vom selben Ausgangszustand:

- Alle 256 Zellen: `value = 2`, `primeIndex = 0`, `colorIndex = ADD`
- Cursor: Position $(0, 0)$

Der Wert 2 ist keine willkürliche Wahl: Es ist die erste Primzahl und
sichergestellt leer von Absorptionszuständen für die AND- und OR-Operationen
in Phase 3 (Null würde durch AND absorbiert, All-Eins durch OR).

---

## 5. Phase 2 — Eingabe-Integration (Fingerprint-Bildung)

Dies ist die **kritische Phase** für Kollisionsresistenz. Hier wird aus der
Eingabe ein eindeutiger Gitterzustand — ein Fingerabdruck — geformt.

### Byte-Zerlegung

Jedes Eingabebyte (8 Bit) wird in vier **2-Bit-Richtungscodes** zerlegt:

| Bits | Richtung       |
|------|----------------|
| `00` | UP (oben)      |
| `01` | RIGHT (rechts) |
| `10` | LEFT (links)   |
| `11` | DOWN (unten)   |

Ein Byte erzeugt also vier Cursorbewegungen. Für jede Bewegung werden
**zwei Operationen** in der aktuellen Zelle $(x, y)$ ausgeführt:

### Operation A — Prime Update

1. `primeIndex` der Zelle wird um 1 erhöht
2. `colorIndex` wird zyklisch weitergeschaltet (0 → 1 → 2 → 3 → 4 → 5 → 0)
3. `value` der Zelle wird mit der nächsten Primzahl aus der Tabelle überschrieben

Durch die zyklische Fortschaltung des `colorIndex` bestimmt die **Reihenfolge**,
in der Zellen besucht werden, welche Operation später in Phase 3 auf sie
angewendet wird — nicht der Inhalt des Eingabebytes direkt.

### Operation B — Nicht-linearer Sprung

Der Cursor bewegt sich zur nächsten Position. Die Sprungweite ist
**datenabhängig**: Sie basiert auf dem **alten Zellwert** (vor dem Prime
Update) und einem richtungsabhängigen Offset:

| Richtung | x-Berechnung                    | y-Berechnung                    |
|----------|---------------------------------|---------------------------------|
| UP       | `x = (x + (y >> 1) + 1) & 15`   | `y = (y - oldPrime + OFF) & 15` |
| DOWN     | `x = (x + (y >> 1) + 1) & 15`   | `y = (y + oldPrime + OFF) & 15` |
| LEFT     | `x = (x - oldPrime + OFF) & 15` | `y = (y + (x >> 1) + 1) & 15`   |
| RIGHT    | `x = (x + oldPrime + OFF) & 15` | `y = (y + (x >> 1) + 1) & 15`   |

(`OFF` ist ein richtungsspezifischer Ganzzahl-Offset, der Wrap-around in
negativen Bereichen verhindert; alle Koordinaten werden mit `& 15` auf
$[0, 15]$ begrenzt.)

**Entscheidend:** Bei vertikalen Bewegungen hängt die neue x-Koordinate
von der neuen y-Koordinate ab und umgekehrt bei horizontalen. Diese
**Achsen-Kopplung** bricht die Kommutativität:

Die Sequenz LEFT→UP erzeugt einen anderen Pfad als UP→LEFT — selbst vom
gleichen Startpunkt. Kombiniert mit den primzahlgesteuerten Sprungweiten
folgen verschiedene Eingaben vollständig verschiedenen Pfaden durch das Gitter.

### Kollisionsresistenz durch Fingerprint-Eindeutigkeit

Zwei Eingaben erzeugen genau dann eine Kollision, wenn sie nach Phase 2
identische (`value`, `primeIndex`, `colorIndex`)-Tupel in **allen 256
Zellen** hinterlassen — trotz verschiedener Pfade, verschiedener
Besuchsreihenfolgen und primzahlgesteuerter Sprungweiten. Die kombinatorische
Komplexität des Zustandsraums ($\approx 2^{16.384}$) macht dies praktisch
unmöglich.

---

## 6. Phase 3 — Verarbeitungsrunden (Diffusion)

Nach der Eingabe-Integration wird das gesamte Gitter $r$ Mal (Standard: $r = 10$)
in Verarbeitungsrunden durchlaufen. In jeder Runde wird **jede Zelle**
aktualisiert — abhängig von ihrem `colorIndex`, der in Phase 2 festgelegt wurde:

| colorIndex | Operation                | Nachbar          |
|------------|--------------------------|------------------|
| 0 — ADD    | `value += Nachbar.value` | oben             |
| 1 — SUB    | `value -= Nachbar.value` | unten            |
| 2 — XOR    | `value ^= Nachbar.value` | links            |
| 3 — AND    | `value &= Nachbar.value` | rechts           |
| 4 — OR     | `value                   | = Nachbar.value` | links |
| 5 — INVERT | `value = ~value`         | —                |

Randbehandlung: An Gitterkanten werden konstante Fallback-Werte (1 oder
unveränderter Wert) verwendet, um undefiniertes Verhalten zu vermeiden.

### Warum sechs verschiedene Operationen?

- **ADD / SUB:** Additive Operationen verteilen Werte global und sind
  invertierbar — sie allein würden lineare Strukturen hinterlassen.
- **XOR:** Bitweise, invertierbar, bricht lineare Korrelationen zwischen
  benachbarten Zellen.
- **AND / OR:** **Nicht invertierbar.** Aus `a AND b = c` lassen sich
  weder $a$ noch $b$ eindeutig rekonstruieren. Diese beiden Operationen
  sind fundamental für die Einwegfunktionseigenschaft: Selbst vollständige
  Kenntnis des Ausgabe-Hashes erlaubt keine Rückrechnung auf den internen
  Zustand.
- **INVERT:** Flipped alle 64 Bits gleichzeitig; verhindert die Konvergenz
  des Feldes zu All-Null- oder All-Eins-Absorptionszuständen.

### Traversierungsreihenfolge

Die Zellen werden nicht in fixer Zeile-für-Zeile-Reihenfolge bearbeitet,
sondern mit einem Versatz, der sich aus der **Endposition des Cursors nach
Phase 2** ergibt. Diese Position ist eingabe-abhängig — die Reihenfolge der
Diffusion selbst variiert also mit der Eingabe.

### Rundenreduktion und Mindestrunden

Die effektive Rundenzahl ist $\max(r,\, \lceil \text{hashBits} / 64 \rceil)$.
Für einen 512-Bit-Hash werden also mindestens 8 Runden ausgeführt — da
pro Runde genau ein 64-Bit-Block extrahiert wird (Phase 4).

Empirisch wurde gezeigt, dass alle Sicherheitsmetriken bereits ab 1 Runde
stabil sind (siehe Runden-Reduktions-Analyse). Dies liegt daran, dass
Kollisionsresistenz und Avalanche-Effekt primär in Phase 2 entstehen — die
Verarbeitungsrunden verstärken die Diffusion, sind aber nicht ihr Ursprung.

---

## 7. Phase 4 — Hash-Extraktion

Nach jeder Verarbeitungsrunde wird ein **64-Bit-Block** aus dem Gitterzustand
extrahiert. Die Extraktionsfunktion iteriert alle 256 Zellen in Zeilen-Haupt-
Reihenfolge und akkumuliert:

$$\text{block} = \bigoplus_{i=0}^{255} \text{ROL}_7\!\left(\text{acc} \oplus (w_i \cdot \text{cell}_i.\text{value})\right)$$

Dabei ist:

- $w_i = i + 1 \in \{1, \ldots, 256\}$ — ein **positionsgebundenes Gewicht**
- $\text{ROL}_7$ — Links-Rotation um 7 Bit nach jedem Schritt
- $\oplus$ — XOR-Akkumulation

Das Positionsgewicht ist entscheidend: Hätten zwei verschiedene Zellen
denselben Wert und wären ihre Positionen getauscht, würde die Multiplikation
mit $w_i$ trotzdem einen anderen Ausgabe-Block erzeugen. Permutationen
identischer Zellwerte sind damit nicht kollisionsäquivalent.

Für einen 512-Bit-Hash werden 8 solche Blöcke aus 8 aufeinanderfolgenden
Runden gesammelt und konkateniert.

---

## 8. Sicherheitsargumente

Secasy ist ein **empirisch evaluierter** Forschungsalgorithmus. Die folgenden
Argumente basieren auf strukturellen Überlegungen und empirischen Messergebnissen
— nicht auf formalen Sicherheitsbeweisen.

### 8.1 Kollisionsresistenz

**Strukturelles Argument:** Zwei verschiedene Eingaben müssten nach Phase 2
identische Zustände in allen 256 Zellen hinterlassen. Der Zustandsraum
umfasst $\approx 2^{16.384}$ mögliche Konfigurationen. Für diese müssten
trotz verschiedener primzahlgesteuerter Pfade und achsengekoppelter Sprünge
exakt übereinstimmen — ein kombinatorisch extrem unwahrscheinliches Ereignis.

**Empirische Bestätigung:** Null Kollisionen in 1.000.000 Versuchen mit
512-Bit-Ausgabe. Der Birthday-Bound liegt bei $2^{256}$ [7] — ein zufälliger
Test ist hier praktisch sinnlos, aber das Fehlen trivialer Schwächen ist
bestätigt.

### 8.2 Preimage-Resistenz (Einwegfunktion)

**Strukturelles Argument:** AND und OR sind nicht invertierbar. Kennt ein
Angreifer den 512-Bit-Hash, kennt er nur 3 % des internen Zustands (512
von 16.384 Bit). Die verbleibenden 97 % (15.872 Bit) müssten erschlossen
werden — bei nicht invertierbaren Operationen und datenabhängiger Traversierung
ist keine algebraische Rückrechnung möglich.

**Empirische Bestätigung:** Keine Preimages in 1.000.000 brute-force-Versuchen.

### 8.3 Length Extension Resistenz

**Strukturelles Argument (inhärent):** Der interne Zustand (16.384 Bit) ist
32× größer als die Ausgabe (512 Bit). Die Ausgabe ist eine verlustbehaftete
XOR-Akkumulation des gesamten Feldes. Ein Angreifer, der $H(m)$ kennt,
besitzt nicht den internen Zustand — er kann die Berechnung nicht fortsetzen,
weil ihm 15.872 Bit fehlen. Dies unterscheidet Secasy fundamental von SHA-256.

**Vergleich:**

| Funktion      | Interner Zustand | Ausgabe     | Verhältnis | Length Ext. anfällig? |
|---------------|------------------|-------------|------------|-----------------------|
| SHA-256 [3]   | 256 Bit          | 256 Bit     | 1:1        | Ja                    |
| SHA-512 [3]   | 512 Bit          | 512 Bit     | 1:1        | Ja                    |
| SHA-3-256 [4] | 1.600 Bit        | 256 Bit     | 6,25:1     | Nein                  |
| **Secasy**    | **16.384 Bit**   | **512 Bit** | **32:1**   | **Nein**              |

### 8.4 Avalanche-Effekt (empirisch bestätigt) [14]

Ein einzelnes geflipptes Eingabe-Bit ändert den Traversierungspfad ab dem
ersten betroffenen Richtungscode. Da die Sprungweite auf dem alten Zellwert
basiert, führt eine andere Zellmodifikation zu einem anderen Sprung, der
zu einer anderen Zellmodifikation führt — ein kaskadierender, nichtlinearer
Effekt. Messungen: 50,0007 % Ausgabe-Bit-Flips bei Einzelbit-Änderungen
(N = 1.000.000, 95%-KI: [49,9963 %, 50,0051 %]).

### 8.5 Nichtlinearität

AND und OR erzeugen nichtlineare Beziehungen zwischen Eingabe und Ausgabe.
Testing: Keine $H(A \oplus B) = H(A) \oplus H(B)$-Instanzen in 10.000
zufälligen Eingangspaaren.

### 8.6 Statistischer Zufall (NIST-inspiriert)

Alle 10 NIST-inspirierten Tests [6] bestanden auf einem Bitstream aus 50.000
verketteten Hashes (6,4 Mio. Bits): Monobit, Runs, Longest Run, Serial,
Approximate Entropy, Cumulative Sums, Byte Distribution, Autocorrelation,
Bit Transition, Hash Collision.

---

## 9. Vergleich mit bekannten Konstruktionen

### 9.1 Abweichung vom Ideal (empirisch)

| Algorithmus  | Avalanche     | Bit-Verteilung | Abweichung vom Ideal |
|--------------|---------------|----------------|----------------------|
| BLAKE2b [5]  | 50,0 %        | 50,01 %        | 0,03 %               |
| SHA-512 [3]  | 49,9 %        | 50,18 %        | 0,06 %               |
| SHA3-256 [4] | 49,9 %        | 50,28 %        | 0,06 %               |
| SHA-256 [3]  | 50,2 %        | 49,87 %        | 0,21 %               |
| **Secasy**   | **50,0007 %** | **50,0007 %**  | **0,0007 %**         |

Secasy zeigt die geringste empirische Abweichung vom theoretischen Ideal.
Dieser Vergleich misst jedoch nur statistische Oberflächeneigenschaften —
er sagt nichts über algebraische Angreifbarkeit aus.

### 9.2 Konstruktionsvergleich

| Eigenschaft                | Merkle-Damgård   | SHA-3 (Sponge)            | Secasy (Gitter) |
|----------------------------|------------------|---------------------------|-----------------|
| Interner Zustand > Ausgabe | Nein             | Ja (6,25:1)               | Ja (32:1)       |
| Length Extension sicher    | Nein             | Ja                        | Ja              |
| Nicht-invertierbare Ops    | Teilweise        | Nein (χ ist invertierbar) | Ja (AND, OR)    |
| Formal bewiesen sicher     | Ja (reduzierbar) | Ja                        | Nein            |
| Peer reviewed              | Ja               | Ja                        | Nein            |
| Rundeninvarianz            | Nein             | Nein                      | Ja (empirisch)  |

### 9.3 Struktureller Unterschied zu AES-basierten Konstruktionen

Bei AES-GCM [9] und ähnlichen Konstruktionen ist jede Runde strukturell
verschieden (Rundenschlüssel). Die Sicherheit ist direkt an die Rundenzahl
gebunden — weniger Runden bedeuten algebraisch einfachere, angreifbare
Transformationen.

Bei Secasy ist die Sicherheit an die **Phase 2** (Eingabe-Integration)
und die **Extraktionsfunktion** gebunden, nicht an die Rundenzahl der
Verarbeitungsphase. Empirisch bestätigt: Alle Sicherheitsmetriken bleiben
von 1 bis 100.000 Runden stabil.

---

## 10. Bekannte Grenzen und offene Fragen

### Was die Tests zeigen — und was nicht

Die empirischen Tests prüfen, ob die Ausgabe **statistisch wie Zufall**
aussieht. Das ist eine notwendige, aber keine hinreichende Bedingung für
kryptographische Sicherheit. Ein linearer Kongruenzgenerator kann perfekte
statistische Tests bestehen und ist dennoch kryptographisch wertlos.

### Nicht getestete Angriffstechniken

| Technik                    | Ziel                               | Status           |
|----------------------------|------------------------------------|------------------|
| Algebraische Angriffe [10] | Polynomdarstellung der Funktion    | Nicht untersucht |
| Meet-in-the-Middle [13]    | Aufteilung der Berechnung          | Nicht untersucht |
| Rebound-Angriffe [11]      | Schwächen in der Diffusionsschicht | Nicht untersucht |
| Cube-Angriffe [12]         | Niedriggrad-Approximationen        | Nicht untersucht |
| SAT-Solver-Angriffe        | Constraint-basierte Preimage-Suche | Nicht untersucht |

### Identifizierte offene Fragen

1. **Formaler Sicherheitsbeweis:** Kein Beweis der Pseudorandom-Permutations-
   Eigenschaft (PRP) oder der Kollisionsresistenz. Ein formaler Beweis würde
   erfordern, die Zustandsübergänge als ergodische Markow-Kette zu modellieren
   und die Mischzeit zu bounded.

2. **AND/OR-Absorptionszustände:** AND zieht Bits gegen 0, OR gegen 1.
   Empirisch wurden keine Absorptionszustände beobachtet (10.000 Tests mit
   strukturierten Eingaben), aber ein formaler Beweis dass ADD, XOR und
   INVERT die Absorption in jedem Fall verhindern, fehlt.

3. **Seitenkanal-Anfälligkeit:** Die aktuelle Implementierung ist nicht
   constant-time. Der `switch(colorIndex)` und der primzahl-indizierte
   Tabellenzugriff erzeugen daten-abhängige Timing- und Cache-Muster. Für
   reine Hashing-Anwendungen (ohne geheime Eingabe) ist dies akzeptabel.
   Als HMAC-Primitiv oder Key-Derivation-Funktion wäre eine constant-time
   Variante erforderlich [8].

4. **Peer Review:** Der Algorithmus wurde noch nicht von unabhängigen
   Kryptographen analysiert. Alle Sicherheitsaussagen sind daher als
   vorläufig zu betrachten.

### Ehrliche Einschätzung

| Frage                                    | Konfidenz               | Basis                                                    |
|------------------------------------------|-------------------------|----------------------------------------------------------|
| Sieht die Ausgabe zufällig aus?          | **Sehr hoch**           | N=1M, Power >99,9 %                                      |
| Ist die Funktion kryptographisch sicher? | **Unbekannt**           | Braucht formale Analyse                                  |
| Gibt es offensichtliche Design-Fehler?   | **Wahrscheinlich nein** | 30+ Tests, 2,5M+ Hashes, 0 Anomalien                     |
| Produktionsreif?                         | **Nein**                | Kein Peer Review, keine formalen Beweise                 |
| Interessanter Forschungsbeitrag?         | **Ja**                  | Neuartiges Konstruktionsprinzip, umfangreiche Evaluation |

---

*Erstellt: 2026-03-15 · Referenz-Implementierung: Secasy 512-Bit, 10 Runden*

---

## 11. Literatur

[1] R. C. Merkle, „A Certified Digital Signature," in *Advances in Cryptology – CRYPTO 1989*, Lecture Notes in Computer
Science, Bd. 435, Springer, Berlin, 1990, S. 218–238.

[2] I. B. Damgård, „A Design Principle for Hash Functions," in *Advances in Cryptology – CRYPTO 1989*, Lecture Notes in
Computer Science, Bd. 435, Springer, Berlin, 1990, S. 416–427.

[3] National Institute of Standards and Technology, *Secure Hash Standard (SHS)*, FIPS PUB 180-4, August 2015. DOI:
10.6028/NIST.FIPS.180-4

[4] National Institute of Standards and Technology, *SHA-3 Standard: Permutation-Based Hash and Extendable-Output
Functions*, FIPS PUB 202, August 2015. DOI: 10.6028/NIST.FIPS.202

[5] J.-P. Aumasson, S. Neves, Z. Wilcox-O'Hearne und C. Winnerlein, „BLAKE2: Simpler, Smaller, Fast as MD5," in *Applied
Cryptography and Network Security – ACNS 2013*, Lecture Notes in Computer Science, Bd. 7954, Springer, Berlin, 2013, S.
119–135.

[6] A. Rukhin et al., *A Statistical Test Suite for Random and Pseudorandom Number Generators for Cryptographic
Applications*, NIST Special Publication 800-22, Rev. 1a, National Institute of Standards and Technology, April 2010.

[7] A. J. Menezes, P. C. van Oorschot und S. A. Vanstone, *Handbook of Applied Cryptography*, CRC Press, 1996.
Verfügbar: http://cacr.uwaterloo.ca/hac/

[8] P. C. Kocher, „Timing Attacks on Implementations of Diffie-Hellman, RSA, DSS, and Other Systems," in *Advances in
Cryptology – CRYPTO 1996*, Lecture Notes in Computer Science, Bd. 1109, Springer, Berlin, 1996, S. 104–113.

[9] National Institute of Standards and Technology, *Advanced Encryption Standard (AES)*, FIPS PUB 197, November 2001.
DOI: 10.6028/NIST.FIPS.197

[10] N. T. Courtois und J. Pieprzyk, „Cryptanalysis of Block Ciphers with Overdefined Systems of Equations," in
*Advances in Cryptology – ASIACRYPT 2002*, Lecture Notes in Computer Science, Bd. 2501, Springer, Berlin, 2002, S.
267–287.

[11] F. Mendel, C. Rechberger, M. Schläffer und S. S. Thomsen, „The Rebound Attack: Cryptanalysis of Reduced Whirlpool
and Grøstl," in *Fast Software Encryption – FSE 2009*, Lecture Notes in Computer Science, Bd. 5665, Springer, Berlin,
2009, S. 260–276.

[12] I. Dinur und A. Shamir, „Cube Attacks on Tweakable Black Box Polynomials," in *Advances in Cryptology – EUROCRYPT
2009*, Lecture Notes in Computer Science, Bd. 5479, Springer, Berlin, 2009, S. 278–299.

[13] W. Diffie und M. E. Hellman, „Special Feature: Exhaustive Cryptanalysis of the NBS Data Encryption Standard,"
*Computer*, Bd. 10, Nr. 6, S. 74–84, Juni 1977.

[14] A. F. Webster und S. E. Tavares, „On the Design of S-Boxes," in *Advances in Cryptology – CRYPTO 1985*, Lecture
Notes in Computer Science, Bd. 218, Springer, Berlin, 1986, S. 523–534.
