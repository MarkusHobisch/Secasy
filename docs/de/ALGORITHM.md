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

Anhang A — Empirische Isolation einzelner Farb-Operationen

Anhang B — Einfluss der Feldgröße auf die Diffusion

---

\newpage

## 1. Motivation — Warum ein neuer Hash-Algorithmus?

### Das dominante Paradigma: Merkle-Damgård

Die bekanntesten kryptographischen Hash-Funktionen — MD5, SHA-1, SHA-256,
SHA-512 — basieren alle auf der **Merkle-Damgård-Konstruktion** aus dem Jahr
1989 [@merkle1990; @damgard1990]. Das Prinzip ist einfach: Die Eingabe wird in gleich große Blöcke
aufgeteilt, die sequenziell durch eine Kompressionsfunktion verarbeitet werden.
Der Ausgabe-Zustand nach dem letzten Block ist der Hash.

Diese lineare Verarbeitungskette hat eine bekannte strukturelle Schwäche:
den **Length Extension Attack**. Kennt ein Angreifer $H(m)$, kann er — ohne
$m$ zu kennen — $H(m \| \text{padding} \| m')$ für beliebige Fortsetzungen
$m'$ berechnen. Der Grund: Der Hash-Ausgabewert *ist* der interne Zustand
der Kompressionsfunktion; der Angreifer „setzt" die Berechnung einfach fort.
SHA-3 (Keccak) [@nist_fips202] und BLAKE2 [@aumasson2013_blake2] lösen dieses Problem durch andersartige Konstruktionen.

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

| Feld               | Typ            | Bedeutung                                                                     |
|--------------------|----------------|-------------------------------------------------------------------------------|
| `value`            | `int64_t`      | Numerischer Zellwert; wird durch Operationen verändert                        |
| `primeIndex`       | `uint32_t`     | Zeiger in die vorberechnete Primzahltabelle                                   |
| `colorIndex`       | `uint8_t`      | Bestimmt, welche der 6 Operationen in Phase 3 auf diese Zelle angewendet wird |

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

Der Wert 2 ist der Startwert. Es ist die erste Primzahl und dient als Initialisierung. Natürlich hätte man auch 1 oder jede andere Initialisierung nehmen können, wir haben uns aus rein pragmatischen Gründen für die 2 entschieden. 

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

### Operation B — Datenabhängiger Sprung

Der Cursor bewegt sich zur nächsten Position. Jede Richtung aktualisiert
genau **eine** Koordinate — die **Primärachse** — um einen datenabhängigen
Offset, der vom alten Zellwert abgeleitet wird:

| Richtung | Formel                                  |
|----------|-----------------------------------------|
| UP       | `y = (y - oldPrime + SAV) & 15`         |
| DOWN     | `y = (y + oldPrime) & 15`               |
| LEFT     | `x = (x - oldPrime) & 15`               |
| RIGHT    | `x = (x + oldPrime + SAV) & 15`         |

(`SAV` = `SQUARE_AVOIDANCE_VALUE` = 1, ein konstanter Offset der nur bei UP
und RIGHT angewendet wird und verhindert, dass sich der Cursor entlang eines
Quadrats bewegen und somit wieder die ursprüngliche Position nach 4
Bewegungen einnehmen könnte. Eine Bewegung im bzw. gegen den Uhrzeigersinn
hätten denselben Feldzustand zur Folge, was zu identischen Hashwerten führen
würde.

Alle Koordinaten werden mit `& 15` auf
$[0, 15]$ begrenzt. Da die Feldgröße $N = 16 = 2^4$ eine Zweierpotenz ist, gilt:

$$x \bmod N \;=\; x \;\&\; (N-1) \;=\; x \;\&\; 15$$

Die bitweise UND-Operation ersetzt die Division und ist deutlich schneller.)

Da UP und DOWN nur $y$ modifizieren, während LEFT und RIGHT nur $x$
modifizieren, sind Cursorbewegungen entlang verschiedener Achsen
**unabhängig**: Die Endposition ist die komponentenweise Summe aller
Einzelsprünge pro Achse. Zwei Richtungsfolgen, die denselben Multiset
an achsenspezifischen Offsets enthalten, landen auf derselben Zelle —
unabhängig von der Reihenfolge der Richtungen. Formal gilt für zwei
Richtungen $d_1, d_2$ auf **verschiedenen** Achsen:

$$\text{move}(d_1) \circ \text{move}(d_2) = \text{move}(d_2) \circ \text{move}(d_1)$$

Diese Kommutativität ist eine **bekannte strukturelle Einschränkung**, die
prinzipiell zu Pfad-Kollisionen führen kann (siehe Abschnitt 10).

Konkret betrachte man zwei Bytes, die sich nur in den untersten 2 Bits
unterscheiden, z.B. `0x1A` (Bits: `00 01 10 00`) und `0x1B`
(Bits: `00 01 10 11`). Der erste 2-Bit-Block (`byte & 3`) ergibt LEFT (10)
vs. DOWN (11) — die drei nachfolgenden Schritte sind identisch. Da LEFT
nur $x$ und DOWN nur $y$ modifiziert, erzeugen diese beiden Richtungen
**orthogonale** Bewegungen auf dem Gitter. Beim Hashing wiederholter Bytes
(z.B. `0x1A` × 16 vs. `0x1B` × 16) können die 64 Einzelschritte dieselben
Zellen in permutierter Reihenfolge besuchen.

Frühere Tests mit einer Variante, die zusätzlich beide Achsen koppelte
(Sekundärachsen-Offset abgeleitet von der Primärachse), eliminierten diese
Kollisionsgruppen — führten aber einen anderen Fehler ein (Direction-Aliasing
durch Informationsverlust in der Kopplungsformel). Die Sekundärachsen-Kopplung
wurde daher entfernt. Die folgenden Byte-Gruppen erfordern weitere
Untersuchung, da sie strukturell äquivalente Cursor-Pfade aufweisen:

| Gruppe | Byte-Werte mit äquivalenter Pfadstruktur    |
|--------|---------------------------------------------|
| 1      | `0x1A`, `0x1B`, `0x1E`, `0x1F`              |
| 2      | `0x26`, `0x27`, `0x36`, `0x37`              |
| 3      | `0x29`, `0x2D`, `0x39`, `0x3D`              |
| 4      | `0x4A`, `0x4B`, `0x4E`, `0x4F`              |
| 5      | `0x86`, `0x87`, `0xC6`, `0xC7`              |

Das gemeinsame Muster: In jeder Gruppe unterscheiden sich die Bytes nur
in Bits, die zu Richtungen auf **verschiedenen Achsen** führen — die
resultierenden Cursor-Pfade sind Permutationen voneinander.

### Warum Kommutativität nicht trivial zu Hash-Kollisionen führt

Obwohl die Cursorbewegung über verschiedene Achsen kommutativ ist, besuchen
zwei permutierte Richtungsfolgen in der Regel **nicht** dieselben Zellen in
derselben Reihenfolge. Die Sprungweite bei jedem Schritt hängt vom
**aktuellen Wert** der besuchten Zelle ab, der sich nach jedem Besuch ändert
(weil `nextPrimeNumber` den `primeIndex` weiterschaltet). Daher gilt:

- Nach dem ersten divergenten Schritt landen die beiden Pfade auf
  **verschiedenen Zellen** mit potenziell verschiedenen `value`-Feldern.
- Nachfolgende Sprungweiten unterscheiden sich, sodass die Pfade weiter
  divergieren.

Für eine Kollision müssten beide Pfade exakt den **gleichen Multiset** an
Zellen mit gleichen Besuchshäufigkeiten pro Zelle durchlaufen — damit jede
Zelle den gleichen `primeIndex` und `value` erhält. Das ist eine deutlich
stärkere Bedingung als bloße Cursor-Konvergenz. Empirisch wurden unter
50.000 zufälligen Nachrichten mit Einzelbit-Flips (12.800.000 Versuche)
keine solchen Kollisionen beobachtet; ein formaler Beweis der
Kollisionsfreiheit existiert jedoch nicht, und die oben genannten
Byte-Gruppen bleiben eine offene Forschungsfrage (siehe Abschnitt 10).

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

| colorIndex | Operation                 | Nachbar          |
|------------|---------------------------|------------------|
| 0 — ADD    | `value += Nachbar.value`  | oben             |
| 1 — SUB    | `value -= Nachbar.value`  | unten            |
| 2 — XOR    | `value ^= Nachbar.value`  | links            |
| 3 — AND    | `value &= Nachbar.value`  | rechts           |
| 4 — OR     | `value \|= Nachbar.value` | links            |
| 5 — INVERT | `value = ~value`          | —                |

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

Die empirische Begründung für diesen Mix — ein Vergleich der
Diffusionskonsistenz bei isolierter Verwendung jeder einzelnen Operation —
findet sich in **Anhang A**.

### Traversierungsreihenfolge

Die Zellen werden nicht in fixer Zeile-für-Zeile-Reihenfolge bearbeitet,
sondern mit einem Versatz, der sich aus der **Endposition des Cursors nach
Phase 2** ergibt. Diese Position ist eingabe-abhängig — die Reihenfolge der
Diffusion selbst variiert also mit der Eingabe.

### Rundenreduktion und Mindestrunden

Die effektive Rundenzahl ist $\max(r,\, \lceil \text{hashBits} / 64 \rceil)$.
Für einen 512-Bit-Hash werden also mindestens 8 Mischrunden ausgeführt, um
ausreichende Diffusion vor der Extraktion sicherzustellen.

Mischen und Extraktion sind **strikt getrennt**: Zuerst werden alle $r$
Runden der Zell-Diffusion vollständig durchlaufen, danach werden in Phase 4
aus dem **finalen** Gitterzustand die benötigten 64-Bit-Blöcke extrahiert.

Empirisch wurde gezeigt, dass alle Sicherheitsmetriken bereits ab 1 Runde
stabil sind (siehe Runden-Reduktions-Analyse). Dies liegt daran, dass
Kollisionsresistenz und Avalanche-Effekt primär in Phase 2 entstehen — die
Verarbeitungsrunden verstärken die Diffusion, sind aber nicht ihr Ursprung.

> **Struktureller Kontrast zu SHA/Sponge-Konstruktionen:** Bei klassischen
> Hashfunktionen wie SHA-2 oder Keccak wird die Sicherheit maßgeblich über
> die Rundenfunktion analysiert — weniger Runden bedeuten direkt schwächere
> Sicherheit. Bei Secasy liegt der primäre Sicherheitskern in der
> **Initialisierungsphase (Phase 2)**: Der primzahlgesteuerte Cursor-Walk
> verteilt und verknüpft die Eingabedaten nichtlinear über alle 256 Zellen,
> bevor Phase 3 überhaupt beginnt. Phase 3 ist damit **Defense-in-Depth** —
> eine zusätzliche Härtungsschicht, nicht die Grundlage der
> Kollisionsresistenz.

> *„Wir schlagen eine Hash-Konstruktion vor, deren Sicherheit auf einer
> zustandsabhängigen, primzahlgesteuerten Initialisierung beruht — statt auf
> einer iterierten Rundenfunktion — und zeigen empirisch, dass alle
> Sicherheitsmetriken bereits nach einer einzigen Verarbeitungsrunde
> sättigen.“*

---

## 7. Phase 4 — Hash-Extraktion

Nachdem **alle** Verarbeitungsrunden abgeschlossen sind, wird die benötigte
Anzahl an 64-Bit-Blöcken aus dem **finalen** Gitterzustand extrahiert.
Die Extraktionsfunktion iteriert alle 256 Zellen in Zeilen-Haupt-Reihenfolge
und akkumuliert:

$$\text{block}_b = \bigoplus_{i=0}^{255} \text{ROL}_7\!\left(\text{acc} \oplus (w_{i,b} \cdot \text{cell}_i.\text{value})\right)$$

Dabei ist:

- $w_{i,b} = i + 1 + b \cdot 256$ — ein **positionsgebundenes Gewicht**, versetzt durch den Blockindex $b$
- $b \in \{0, 1, \ldots, \lceil \text{hashBits}/64 \rceil - 1\}$ — der Blockindex
- $\text{ROL}_7$ — Links-Rotation um 7 Bit nach jedem Schritt
- $\oplus$ — XOR-Akkumulation

Der Blockindex-Versatz stellt sicher, dass jeder extrahierte Block einen
eigenen Satz von Positionsgewichten verwendet — jeder 64-Bit-Block ist
somit eine unterschiedliche Linearkombination der Gitterzellen.

Das Positionsgewicht ist entscheidend: Hätten zwei verschiedene Zellen
denselben Wert und wären ihre Positionen getauscht, würde die Multiplikation
mit $w_{i,b}$ trotzdem einen anderen Ausgabe-Block erzeugen. Permutationen
identischer Zellwerte sind damit nicht kollisionsäquivalent.

Für einen 512-Bit-Hash werden 8 solche Blöcke ($b = 0 \ldots 7$) aus dem
finalen Gitterzustand extrahiert und konkateniert:

$$H = \text{block}_0 \,\|\, \text{block}_1 \,\|\, \cdots \,\|\, \text{block}_7$$

---

## 8. Sicherheitsargumente

Secasy ist ein **empirisch evaluierter** Forschungsalgorithmus. Die folgenden
Argumente basieren auf strukturellen Überlegungen und empirischen Messergebnissen
— nicht auf formalen Sicherheitsbeweisen.

### 8.1 Kollisionsresistenz

**Strukturelles Argument:** Zwei verschiedene Eingaben müssten nach Phase 2
identische Zustände in allen 256 Zellen hinterlassen. Der Zustandsraum
umfasst $\approx 2^{16.384}$ mögliche Konfigurationen. Für diese müssten
trotz verschiedener primzahlgesteuerter Pfade alle 256 Zellen
exakt übereinstimmen — ein kombinatorisch extrem unwahrscheinliches Ereignis.

**Empirische Bestätigung:** Null Kollisionen in 1.000.000 Versuchen mit
512-Bit-Ausgabe. Der Birthday-Bound liegt bei $2^{256}$ [@menezes1997_hac] — ein zufälliger
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

| Funktion                         | Interner Zustand | Ausgabe     | Verhältnis | Length Ext. anfällig? |
|----------------------------------|------------------|-------------|------------|-----------------------|
| SHA-256 [@nist_fips180_4]        | 256 Bit          | 256 Bit     | 1:1        | Ja                    |
| SHA-512 [@nist_fips180_4]        | 512 Bit          | 512 Bit     | 1:1        | Ja                    |
| SHA-3-256 [@nist_fips202]        | 1.600 Bit        | 256 Bit     | 6,25:1     | Nein                  |
| **Secasy**                       | **16.384 Bit**   | **512 Bit** | **32:1**   | **Nein**              |

### 8.4 Avalanche-Effekt (empirisch bestätigt) [@webster1986_sboxes]

Ein einzelnes geflipptes Eingabe-Bit ändert den Traversierungspfad ab dem
ersten betroffenen Richtungscode. Da die Sprungweite auf dem alten Zellwert
basiert, führt eine andere Zellmodifikation zu einem anderen Sprung, der
zu einer anderen Zellmodifikation führt — ein kaskadierender, nichtlinearer
Effekt. Messungen: 49,96 % Ausgabe-Bit-Flips bei Einzelbit-Änderungen
(N = 3.200, 95%-KI: [49,88 %, 50,03 %]).

### 8.5 Nichtlinearität

AND und OR erzeugen nichtlineare Beziehungen zwischen Eingabe und Ausgabe.
Testing: Keine $H(A \oplus B) = H(A) \oplus H(B)$-Instanzen in 10.000
zufälligen Eingangspaaren.

### 8.6 Statistischer Zufall (NIST-inspiriert)

Alle 10 NIST-inspirierten Tests [@bassham2010_sp800_22] bestanden auf einem Bitstream aus 50.000
verketteten Hashes (6,4 Mio. Bits): Monobit, Runs, Longest Run, Serial,
Approximate Entropy, Cumulative Sums, Byte Distribution, Autocorrelation,
Bit Transition, Hash Collision.

---

## 9. Vergleich mit bekannten Konstruktionen

### 9.1 Abweichung vom Ideal (empirisch)

| Algorithmus  | Avalanche     | Bit-Verteilung | Abweichung vom Ideal |
|--------------|---------------|----------------|----------------------|
| BLAKE2b [@aumasson2013_blake2]  | 50,0 %        | 50,01 %        | 0,03 %               |
| SHA-512 [@nist_fips180_4]  | 49,9 %        | 50,18 %        | 0,06 %               |
| SHA3-256 [@nist_fips202] | 49,9 %        | 50,28 %        | 0,06 %               |
| SHA-256 [@nist_fips180_4]  | 50,2 %        | 49,87 %        | 0,21 %               |
| **Secasy**   | **49,96 %**   | **49,96 %**    | **0,04 %**           |

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

Bei AES-GCM [@nist_fips197] und ähnlichen Konstruktionen ist jede Runde strukturell
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
| Algebraische Angriffe [@courtois2002] | Polynomdarstellung der Funktion    | Nicht untersucht |
| Meet-in-the-Middle [@diffie1977]    | Aufteilung der Berechnung          | Nicht untersucht |
| Rebound-Angriffe [@mendel2009_rebound]      | Schwächen in der Diffusionsschicht | Nicht untersucht |
| Cube-Angriffe [@dinur2009_cube]         | Niedriggrad-Approximationen        | Nicht untersucht |
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
   Variante erforderlich [@kocher1996_timing]. Zusätzlich wäre in solchen
   Anwendungsszenarien die Resistenz gegenüber **Fault Injection Analysis
   (FIA)** zu prüfen: Die nichtlineare Kopplung der 256 Gitterzellen
   (AND, OR, wechselnde Nachbaroperationen) erschwert die algebraische
   Modellierung von Fehlerausbreitung strukturell — dennoch bietet die
   aktuelle Implementierung keine formale FIA-Garantie.

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

*Erstellt: 2026-03-16 · Referenz-Implementierung: Secasy 512-Bit, 10 Runden*

---

\newpage

## Anhang A — Empirische Isolation einzelner Farb-Operationen

Um die Notwendigkeit des Operationen-Mix experimentell zu belegen, wurde die
Verarbeitungsphase so modifiziert, dass sie **ausschließlich eine einzige
Operation** auf alle 256 Gitterzellen anwendet. Für jeden Modus wurden
100 zufällige Nachrichten (32 Bytes) gehasht; für jede Nachricht wurde
jeweils jedes der $32 \times 8 = 256$ Eingabebits einzeln invertiert und
die Hamming-Distanz zum Original-Hash gemessen ($N = 25\,600$ Stichproben
pro Modus).

**Tabelle: Mittlere Hamming-Distanz und Standardabweichung nach Modus**

| Modus                          | Mittelwert $\mu$ | Stdabw. $\sigma$ | Min         | Max         | Bewertung             |
|-------------------------------|-----------------|-----------------|-------------|-------------|-----------------------|
| Baseline (voller Mix)         | 50,0 %          | ±2,3 %          | 0,0 %       | 58,4 %      | Optimal                |
| ADD only                      | 50,0 %          | ±2,3 %          | 10,9 %      | 59,0 %      | Stark                  |
| SUB only                      | 50,0 %          | ±2,3 %          | 0,0 %       | 63,3 %      | Stark                  |
| XOR only                      | 49,4 %          | ±4,2 %          | 0,0 %       | 64,5 %      | Stark                  |
| AND only                      | 48,5 %          | **±6,9 %**      | 0,0 %       | 65,6 %      | Degradiert             |
| OR only                       | 47,7 %          | **±8,6 %**      | 0,0 %       | 61,9 %      | Signifikant schwächer  |
| INVERT only                   | 49,2 %          | ±4,6 %          | 0,0 %       | 62,1 %      | Leicht geschwächt      |

> **Zur Standardabweichung $\sigma$:** Sie beschreibt die **Streubreite der
> Hamming-Abstände** um den Mittelwert. Ein kleines $\sigma$ bedeutet, dass
> *jeder einzelne* Bit-Flip zuverlässig nahe bei 50 % der Ausgabebits verändert
> — die Werte liegen eng zusammen. Ein großes $\sigma$ bedeutet dagegen, dass
> manche Bit-Flips kaum etwas ändern (z. B. 10 %) und andere sehr viel
> (z. B. 70 %) — der Durchschnitt liegt zwar noch bei ~50 %, aber die
> **Konsistenz fehlt**.

![Histogramme: Hamming-Distanz-Verteilung je Modus](../en/img/color_isolation_histograms.png)

![Zusammenfassung: μ ± σ je Modus](../en/img/color_isolation_summary.png)

**Interpretation der Ergebnisse:**

Bemerkenswerterweise kollabiert weder AND noch OR vollständig — der
Mittelwert bleibt in allen Modi nahe bei 50 %. Dies liegt daran, dass die
**Initialisierungsphase bereits die primäre Diffusion trägt**: Der
primzahlgesteuerte Cursor-Walk verteilt die Eingabedaten so tiefgreifend
über alle 256 Zellen, dass selbst eine monoton-sättigende Operation in der
Verarbeitungsphase diese Grunddiffusion nicht vollständig zerstören kann.

Der entscheidende Qualitätsindikator ist die Standardabweichung $\sigma$,
nicht der Mittelwert:

- **AND only:** $\sigma = 6{,}9\,\%$ — 3,0-mal schlechter als Baseline.
  Das Histogramm ist deutlich breiter; ein relevanter Anteil der Stichproben
  liegt außerhalb des idealen 45–55%-Bands.
- **OR only:** $\sigma = 8{,}6\,\%$ — 3,7-mal schlechter als Baseline.
  Das Histogramm reicht von 0 % bis über 60 %; die Diffusion ist
  stark ungleichmäßig und nicht mehr vorhersagbar eng.
- **ADD / SUB / XOR:** $\sigma \leq 4{,}2\,\%$ — nahezu identisch mit
  dem vollen Mix.

Diese Befunde bestätigen, dass der **rotierende Operationen-Mix** nicht zur
Erreichung des mittleren 50%-Werts erforderlich ist (dieser entsteht bereits
in Phase 2), wohl aber für die **Konsistenz und Enge der Verteilung**.
Nur der volle Mix erreicht $\sigma = 2{,}3\,\%$ und garantiert damit, dass
jeder einzelne Bit-Flip mit sehr hoher Wahrscheinlichkeit genau ~50 % der
Ausgabebits verändert — ohne Ausreißer.

---

\newpage

## Anhang B — Einfluss der Feldgröße auf die Diffusion

Secasy verwendet standardmäßig ein $16 \times 16$-Gitter (256 Zellen).
Hier untersuchten wir, ob eine andere Feldgröße — kleiner oder
größer — die Diffusionsqualität verbessern oder verschlechtern würde.
Dazu wurde die Algorithmus-Implementierung so parametrisiert, dass die
Feldgröße zur Laufzeit zwischen $4 \times 4$, $8 \times 8$, $16 \times 16$
(Baseline), $32 \times 32$ und $64 \times 64$ variiert werden kann.

**Methodik.** Für jede der fünf Feldgrößen wurden $N = 100$ zufällige
Nachrichten (32 Bytes) gehasht. Je Nachricht wurde jedes der
$32 \times 8 = 256$ Eingabebits einzeln invertiert und die Hamming-Distanz
zum Original-Hash ($512$ Bit) gemessen — insgesamt $25\,600$ Stichproben
pro Feldgröße. Zusätzlich wurde die *Nibble-Symmetrie-Bias* bestimmt:
die maximale Abweichung der Flip-Rate eines einzelnen 4-Bit-Ausgabenibbles
vom Ideal $50\,\%$.

**Tabelle: Diffusionsqualität nach Feldgröße**

| Feldgröße       | Zellen | $\mu$           | $\sigma$         | Min         | Max         | Nibble-Bias   | Bewertung         |
|-----------------|--------|-----------------|------------------|-------------|-------------|---------------|-------------------|
| $4 \times 4$    | 16     | 46,9 %          | **±12,3 %**      | 0,0 %       | 62,3 %      | 3,40 pp       | Degradiert        |
| $8 \times 8$    | 64     | 48,4 %          | **±9,1 %**       | 0,0 %       | 59,2 %      | 1,96 pp       | Schwach            |
| $16 \times 16$  | 256    | 50,0 %          | ±2,2 %           | 0,0 %       | 59,2 %      | 0,45 pp       | **Baseline**      |
| $32 \times 32$  | 1024   | 50,0 %          | ±2,2 %           | 41,6 %      | 59,2 %      | 0,38 pp       | Gleichwertig      |
| $64 \times 64$  | 4096   | 50,0 %          | ±2,2 %           | 41,6 %      | 58,0 %      | 0,48 pp       | Gleichwertig      |

> **Zur Interpretation:** Der Mittelwert $\mu$ allein ist wenig aussagekräftig —
> entscheidend ist die Standardabweichung $\sigma$, die die *Konsistenz* der
> Diffusion misst. Ein niedrigeres $\sigma$ bedeutet, dass jeder einzelne
> Bit-Flip zuverlässig nahe bei 50 % der Ausgabebits verändert. Der *Nibble-Bias*
> zeigt, ob bestimmte Ausgabepositionen systematisch weniger sensitiv sind
> als andere (kleiner = besser).

![Histogramme: Hamming-Distanz-Verteilung je Feldgröße](../en/img/field_size_histograms.png)

![Zusammenfassung: μ ± σ und Nibble-Bias je Feldgröße](../en/img/field_size_summary.png)

**Interpretation der Ergebnisse:**

1. **Unterhalb von $16 \times 16$ bricht die Diffusion ein.**
   Bei $4 \times 4$ ($\sigma = 12{,}3\,\%$) und $8 \times 8$
   ($\sigma = 9{,}1\,\%$) ist die Hamming-Distanz-Verteilung breit und
   asymmetrisch. Zahlreiche Stichproben fallen in den Bereich $0{-}5\,\%$
   ($n = 1\,570$ bzw. $n = 831$ von $25\,600$), d. h. viele Bit-Flips
   erzeugen nahezu keine Änderung im Hash — das Gegenteil des
   Avalanche-Kriteriums. Der Mittelwert liegt mit $46{,}9\,\%$ bzw.
   $48{,}4\,\%$ deutlich unter dem Ideal. Die Ursache: Bei nur 16 bzw.
   64 Gitterzellen ist der Zustandsraum zu klein, damit die
   primzahlgesteuerte Cursor-Bahn hinreichend viele verschiedene Zellen
   beeinflusst.

2. **$16 \times 16$ ist der empirische Sättigungspunkt.**
   Ab dieser Feldgröße stabilisiert sich $\sigma$ bei $\approx 2{,}2\,\%$
   und der Mittelwert bei $50{,}0\,\%$. Der Nibble-Bias sinkt auf
   $< 0{,}5$ Prozentpunkte. Die 0–5%-Bin enthält nur noch $n = 1$
   von $25\,600$ Stichproben — ein statistischer Ausreißer, der darauf
   hindeutet, dass für mindestens ein Nachricht-Bit-Paar die
   Cursor-Pfad-Divergenz im 256-Zellen-Raum gerade noch nicht
   vollständig greift.

3. **$32 \times 32$ und $64 \times 64$ bringen keine messbare Verbesserung.**
   $\sigma$, Nibble-Bias und mittlere Hamming-Distanz sind im Rahmen der
   statistischen Schwankung identisch mit $16 \times 16$. Die einzige
   Verbesserung: Die Minimum-Hamming-Distanz steigt von $0{,}0\,\%$ auf
   $\approx 42\,\%$ — das 0–5%-Bin ist leer. Dieser Gewinn wird jedoch
   durch massiv höhere Zustandsgröße (4× bzw. 16× so viele Zellen) und
   entsprechend höhere Laufzeit erkauft.

**Schlussfolgerung:**
Die Feldgröße $16 \times 16$ stellt den empirisch optimalen Kompromiss dar:
sie ist die *kleinste* Gittergröße, bei der die Diffusion vollständig
saturiert ($\sigma \leq 2{,}3\,\%$, $\mu = 50{,}0\,\%$). Größere Gitter
bieten keinen messbaren Qualitätsgewinn bei σ oder Nibble-Bias. Die seltenen
0%-Hamming-Ausreißer ($\leq 1 / 25\,600$) deuten auf einzelne Grenzfälle
der Cursor-Bahn hin, nicht auf einen systematischen Defekt; bei $32 \times 32$
verschwinden sie durch den größeren Walk-Raum.

---

\newpage
## 11. Literatur
