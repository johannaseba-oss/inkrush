# Inkrush – mobiler Tintenkatzen-Runner (UE 5.8, iPhone Hochformat)


## Flüche der Gegner-Würfel
Schwarze Würfel kosten kein Leben mehr, sondern geben 10 s einen zufälligen Fluch (`Buffs.h`): `UBuff_CurseControls`
(links/rechts vertauscht, GameMode::RequestLaneShift), `UBuff_CurseMirror` (Bild gespiegelt, `PP_Mirror` über `ARunCamera::SetMirror`),
`UBuff_CurseSpeed` (doppeltes Tempo, `ATrackDirector::SpeedCurse`). Test: `-CatCurseAt=<s>`.
Schnellzüge: 20 % der fahrenden Züge mit 11–15 m/s. Stehende Züge tragen immer eine Münzreihe auf dem Dach (nur per Doppelsprung).

## Züge als 3D-Modelle (Stand)
Lok, Waggon und Lore: `Tools/Import/Nature/3d/*.obj` (Tripo, Vertexfarben) -> `Tools/blender_prepare_models.py` -> FBX ->
Setup-Schritt `models` -> `/Game/Models/SM_TrainLoco`, `SM_TrainCar`, `SM_Minecart` (Material `M_VertexMono`).
Züge: zwei Gleise breit, Modelle gleichmäßig skaliert (Waggon ~480 x 266 x 372 cm, Lok ~358 x 266 x 306 cm); Lok + 1–3 Waggons
(fährt entgegen oder steht) oder nur Waggons (stehen). Zug-Abschnitt: 2 Züge je Zone, Parkour: 1.
Nach Button-Tipps (Item, Weiter, Start) setzt `ACatRunPlayerController::ResetInputAfterUi` den Touch-Zustand zurück;
Buttons sind nicht fokussierbar (Tastatur bleibt im Spiel).
Rampen/Blöcke/Stufen sind entfernt (Abgründe bleiben). Loren pendeln quer (drüberspringbar).
Bombe sprengt auch Züge im Bereich; Tintenfläche liegt flach über allen Gleisen (Instanzdaten: offen links/rechts, Mitte, halbe Breite).
Einstellungen: Regler MUSIK/SOUND im Menü und in der Pause (gespeichert). Zuggeräusch folgt dem Abstand zum nächsten fahrenden Zug.
Spraydose: Sprühnebel-Overlay unten (`Spray Overlay.png`, `Tools/blender_make_spray_overlay.py`), bleibt 2 s nach dem Flug.

## Zug-Abschnitte
Im Wechsel: 120–220 m nur Züge (2 Züge je Zone, 65 % fahren entgegen), dann 180–300 m Parkour (Rampen/Blöcke, wenige
Züge). Fahrende Züge starten so weit vorn, dass sie erst hinter dem letzten Gelände ihrer Fahrbahnen ankommen.
Zugfront: `Train front.png`. Kisten sind wieder Bilder (`Crate`/`Cardboard box`). Game-Over-Klang: `Geme over.wav`.
Das Blueprint `BP_TrackDirector` hatte die alte Wolken-Klasse gespeichert -> `[CoreRedirects]` in DefaultEngine.ini.

## Spraydose (ersetzt die Tintenwolke)
`UBuff_SprayPaint`: weiße 3D-Dose (Zylinder) auf dem Rücken, schwarze Tropfen/Nebel sprühen nach hinten, die Katze steigt
auf die Luft-Ebene (`AirLevelZ` 620 cm, 5 Luft-Fahrbahnen = gleiche Lagen wie unten). Dort keine Hindernisse
(Kollisionen werden übersprungen), nur Münzreihen von 10–20 (`OnSprayFlight`). Nach dem Flug 1,2 s unverwundbar, mit
Shop `SICHERE LANDUNG` 3 s. Flug 5 s (+Shop). Sound `spraypaint.wav`; Icon `Tools/blender_make_spray_icon.py`.
Züge sind 2 Fahrbahnen breit (`ATrackPlatform::Span`/`Covers`), Häuser auf der Strecke und querfahrende Autos sind entfernt.
Treffer: `take damage 1..3.wav`; entgegenkommender Zug: `Train.wav`. Test: `-CatSprayAt=<s>`.

## 3D-Hindernisse (schlichte Formen)
Geländezonen alle 14–32 m (ab 30 m), je 2–4 Fahrbahnen belegt, eine bleibt frei. Pro Fahrbahn: 24 % Zug mit Rampe
(Dach begehbar, 240–270 cm), 26 % Zug ohne Rampe (290–320 cm, zu hoch: ausweichen; 60 % davon kommen mit 3,5–6,5 m/s
entgegen, Scheinwerfer vorn), 16 % Haus auf der Fahrbahn (Wand), sonst helle Rampe/Erhöhung. `ATrackPlatform`:
`SetupTrain`/`SetupHouse`, `StepPlatform` (Bewegung), `SweepA` hält die Fahrbahn bis zur Begegnung frei.
Zaun und Kiste sind jetzt 3D (Absperrung mit Streifen, Kiste mit Kreuz); 2D-Bilder nur noch als Deko/bewegte Deko.

## Stand: nur Endlos-Modus
Das Tutorial (Rundkurs) wurde entfernt; das Menü hat SPIELEN und SHOP. Abschnitte zu Rundkurs/Tutorial weiter unten sind
nur noch historisch (der Layout-Code für Kurven existiert noch, wird aber nicht mehr benutzt).
Sounds: `jump 1..n.wav` werden zufällig abgewechselt (leiser: `JumpVolume`/`LaneVolume` in `ACatAudio`),
`Car honk.wav` hupt einmal, kurz bevor die Katze ein querfahrendes Auto erreicht.
Projekt: `C:\Users\johan\Documents\Unreal Projects\ShadowCat\ShadowCat.uproject` (Ordner/Modul heißen technisch weiter ShadowCat).
Map: `/Game/Maps/Run` (leer – Level, Licht, Nebel, Katze und Kamera entstehen zur Laufzeit).

## Modi (Stand 28.09.)

- **ENDLOS** (Hauptmodus): endlose gerade Strecke mit 5 Fahrbahnen. Score = eingefärbte Abschnitte + Gegnerboni,
  Rekord wird gespeichert. Am Horizont geht ein riesiger Tintengegner mit (Platzhalter, bis das Modell kommt) und wirft
  alle 9–15 s (später häufiger) weiße Tinte: grauer Zielring als Vorwarnung, dann weiße Pfütze auf 1–2 Fahrbahnen für
  2,8 s – Hineinlaufen kostet ein Leben (Drüberspringen geht). Gezielt wird nur auf Fahrbahnen ohne Hindernis dort.
- **Salve** (Endlos, ca. jede Minute, erste nach 50 s): der Riese wirft 6 Reihen schnell hintereinander, jede sperrt 3 der
  5 Fahrbahnen; frei bleiben zwei benachbarte, der freie Weg wandert pro Reihe höchstens eine Fahrbahn weiter (Zickzack).
  Alle Zielringe erscheinen sofort, der Bereich ist frei von Hindernissen, Pfützen liegen, bis die Katze vorbei ist.
  Einstellbar: `BP_TrackDirector → Riese → Salvo*`. Test: `-CatSalvoAt=12`.
- **Münzen** (Endlos): schwarze schwebende Münzen mit hellem Rand, Zehnerreihen oder einzeln, nur auf freien Fahrbahnen
  (erst gelegt, wenn die Hindernisse dort geplant sind; weiße Tinte spült sie weg). Anzahl im HUD, Summe im Menü und
  im Spielstand (`UCatRunSaveGame::Coins`). Klasse `ACoinField` (2 instanzierte Meshes). Drüberspringen = verpasst.
- **Sarg**: Deckel trägt die Katze – früh springen = oben landen und drüberlaufen, knapp springen = sie klettert hinauf.
  Sprung kurz vor der Landung getippt wird vorgemerkt. Test: `-CatScenario=coffin -CatMode=endless`.
- **Item-Symbole**: `Tools/Import/UI/*.png` → `/Game/UI/T_Icon_*` (`CAT_SETUP=ui`), `UCatBuff::Icon`; nur das Symbol
  rechts am Rand (ohne Kreis), unsichtbar wenn leer, antippen = einsetzen. Auslosung 0,8 s.
- **Salven werden schwerer**: jede weitere Salve +1 Reihe (7 → max. 10), Reihen dichter (13 m → min. 10 m), schneller
  geworfen, und immer öfter bleibt nur eine Fahrbahn frei (25 % → 85 %). Abstand 45–60 s.
- **Kulisse aus Bildern**: keine 3D-Deko mehr. `Tools/Import/Nature/Tree*.png`, `House*.png` → `/Game/Nature/T_Sprite_*`
  (`CAT_SETUP=sprites`), aufrechte Bildtafeln (`M_Sprite`, maskiert), die sich zur Kamera drehen. Weitere Bilder
  (Tree 2.png, House 2.png, …) einfach dazulegen – bis zu 9 je Sorte werden zufällig gemischt.
- **Bildaufbau (Endlos)**: Djinn-Video mittig hinter der Strecke (9000 cm voraus, 70 m groß), davor eine Wolkenbank
  aus weißen und schwarzen Schwaden. Links und rechts dicht an der Straße große Häuser (10,5–14 m), Laternenpaare direkt
  am Rand, dazwischen Kleinkram (`bush`, `car`, `Cardboard box`, `stopsign`, `Crate`), dahinter Bäume inkl. `pinetree`.
  Tafeln auf der Geraden drehen sich höchstens 28° zur Kamera (wirken wie Kulissenwände). Nebel: helle Schwaden am
  Straßenrand, flacher Bodennebel über der Strecke, dunkle Schwaden weiter draußen (`M_FogPuff`-Parameter `Color`).
- **Riese = Videos**: `Tools/Import/Enemy mp4/*.mp4` → `Content/Movies/` (Schritt `sprites`), große additive Tafel am
  Himmel (`M_Video`, schwarzer Hintergrund verschwindet). Ruhe = „Enemy idle“ und „Enemy idle 2“ im Wechsel,
  Einzelwurf = „Enemy prepares salve“, Salve = „prepares salve“ → „Enemy salve“ (Würfe starten `SalvoPrepareTime`
  später), Leben verloren = „Cat got hit“, Katze tot = „Player dead“; zwischen allen Videos Überblendung über Schwarz
  (`ABossGiant::FadeTime`). Größe: `VideoSize`. Video-Wechsel stehen im Log („Riese-Video: …“).
- Deko-Bilder zusätzlich: `street lamp.png` (Laternen am Straßenrand). Deko hält Abstand zueinander (nichts steht in/auf etwas).
- **Parkour-Hindernisse** (ersetzen die alten 3D-Hindernisse): **Zaun** (`Zaun.png`, überspringen oder ausweichen),
  **Laterne** (auf der Fahrbahn, zu hoch → ausweichen), **Abgrund** (3–4,3 m, überspringen; wer am Boden hineinläuft,
  verliert ein Leben), **Balken** (Unterkante 75 cm → Wisch nach unten = kriechen, 0,8 s). „Wände“ über alle Fahrbahnen
  (Abgrund/Balken/Zaun) ab 50 m, Anteil 12 % → 35 %; danach immer `ActionRecoverTime` Luft. Einstellbar:
  `BP_TrackDirector → Difficulty → Parkour`. Wisch nach unten in der Luft = schnell landen und direkt kriechen.
  Kriechen = `A_CatCrawl` (Quelle `Tools/Import/Cat/Source/crawling.fbx`, gebacken mit blender_prepare_cat.py, `CAT_SETUP=cat,materials`).
- **Abgründe sind echte Löcher** (nur Endlos): auf ganze 3-m-Abschnitte ausgerichtet, die Fahrbahn-Kacheln verschwinden
  (`AInkCanvas::SetHole`), darunter ein 5 m tiefer Schacht. Unter den Fahrbahnen liegt kein Unterbau mehr. Wer hineinläuft,
  stürzt hinein (`ARunnerCat::StartFall`), verliert ein Leben und kommt hinter dem Loch wieder hoch. Im Tutorial → Zäune.
- **Deko wird Hindernis** (Endlos, ab 60 m, alle 90–170 m, nur auf flacher Strecke): Baum/Haus/Laterne in voller
  Kulissengröße gleitet dauerhaft und gleichmäßig von Rand zu Rand hin und her (2,2–3,2 m/s). Treffen kann nur der Fuß
  (Stamm, Pfahl, Hauswand) → Moment abpassen (`ASlidingProp`).
- Nebeneinanderliegende Abgründe (gleich lang je Reihe, Schlucht in gleichen Stücken) haben keine Zwischenwände,
  auch nicht zwischen hintereinanderliegenden Stücken derselben Fahrbahn (`UpdatePitWalls`).
- Bewegte Hindernisse alle 35–70 m (nur flache Strecke; gewöhnliche Hindernisse im Abschnitt werden dafür entfernt):
  Autos fahren einmal quer über die Straße (3,8–5,2 m/s, beim Eintreffen der Katze irgendwo auf der Fahrbahn),
  Kisten (drüberspringbar) und Stoppschilder pendeln von Rand zu Rand.
- **Pause** (Taste oben links, P/Esc): Spiel angehalten, Seite mit WEITER / SHOP / LAUF BEENDEN. Im Shop sind die
  Münzen des laufenden Laufs schon verfügbar; Käufe wirken sofort (Leben, Wolke, Doppelsprung, Reichweite, +1 Bombe).
- **Tintenbombe**: Reichweite 18 m, Shop-Upgrade je +6 m (bis 42 m). Räumt alles im Weg: Hindernisse, bewegte Deko,
  Würfel, weiße Tinte, Würfe – und schüttet Löcher zu. Optik: `M_InkSheet`, eine zusammenhängende glänzend schwarze
  Fläche aus überlappenden Stücken, die dem Gelände folgen; nur echte Außenkanten fransen aus (Instanzdaten 0/1).
- **HUD**: oben links Pause + Score/Zeit/Leben, oben rechts große Münzanzeige (Guthaben), rechts ein abgerundetes
  Inventar mit Wolke und Bombe (antippen = einsetzen). Test: `-CatPauseTest=<ordner>`.
- Erdkrümmung: alle Oberflächen-Materialien bekommen in `ue_setup.py` (`add_curvature`, `CURVE_K`) einen
  World-Position-Offset, der mit dem Abstand zur Kamera quadratisch absinkt (reine Darstellung).
- **Kiste** (`Tools/Import/Nature/Crate.png`, `T_Sprite_Crate`): Sprung-Hindernis (auch als Wand über alle Fahrbahnen).
- **Gelände**: 1–4 Fahrbahnen, Rampen bis 2,6 m, Blöcke bis 1,6 m, oft eine höhere Stufe dahinter (vom Plateau
  hochspringen, bis ~3 m). Mehr Hindernisse (kürzere Abstände, öfter mehrere Fahrbahnen gesperrt).
- **Shop** (Startbildschirm → SHOP): Münzen ausgeben. +1 Leben (150, Preis verdoppelt sich, max. 15 Leben),
  Wolke +1 s (max. +5), Flug unverwundbar (ohne: auf der Wolke nur hoch, nicht geschützt), Doppelsprung,
  Start-Bombe (max. 3). Gespeichert im Spielstand (`UCatRunSaveGame::Upg*`), Münzen = Guthaben. Test: `-CatShopShot=a.png`.
- Tintenbombe flächendeckend: dichte, überlappende Kleckse über alle Fahrbahnen der ganzen Reichweite.
- Tintenbombe: Reichweite 40 m, auch schon geworfene weiße Tinte landet nicht mehr.
- **Gelände** (Endlos, ab 70 m, alle 60–120 m, auf 1–3 Fahrbahnen, mind. zwei bleiben flach): Rampe hoch auf ein
  Plateau (110–170 cm) oder Erhöhung ohne Rampe (85–110 cm, draufspringen). Oben weiterlaufen, am Ende herunterfallen.
  Gegen die Wand laufen (vorn ohne Rampe oder seitlich hineinwechseln) kostet ein Leben (`ATrackPlatform`). Auf
  Gelände-Fahrbahnen gibt es keine Hindernisse/Münzen. Test: `-CatParkourEarly`.
- **Spur = Pfotenabdrücke** (`Tools/Import/Items/Cat Paw print.png` → `/Game/Fx/T_Item_CatPawPrint`, Schritt `sprites`):
  abwechselnd links/rechts alle 46 cm, 110 cm hinter der Katze, nur wo sie wirklich aufgetreten ist (nicht im Sprung/
  Sturz), auch auf Plateaus (`AInkMarks`, `BP_TrackDirector → Spur`). Die Einfärbung wird weiter gewertet, aber nicht mehr
  als schwarze Fläche gezeigt (`AInkCanvas::bShowCoverage`). Im Rundkurs bleiben 3200 Abdrücke liegen (zeigt Gefärbtes).
- **Items stapelbar**: jede Sorte hat einen Zähler (bis 9), rechts je Sorte Symbol + „x2“; antippen setzt eines ein.
  Tastatur: E = Tintenbombe, Q/Shift = Tintenwolke.
- **Tintenbombe**: sprengt bis 16 m vor der Katze alle Hindernisse (außer Löcher), Würfel, Deko-Hindernisse und weiße
  Tinte weg (+10 Punkte je Stück), animierte schwarze Kleckse nach vorn. Gestapelt: jede Bombe sprengt erneut.
- **Tintenwolke**: die Katze fliegt 4 s auf einer schwarzen Wolke (2,1 m hoch, „flying idle“-Animation) und kann nicht
  getroffen werden (was sie berührt, zerbricht). Färbt nicht mehr ein. Gestapelt: Flug verlängert.
- **Salven**: manche Reihen sperren alle 5 Fahrbahnen → drüberspringen (18 % → 50 % je Salve, nie zwei hintereinander,
  danach mehr Abstand).
- **Größere Abgründe**: 3 oder 6 m; **Schluchten** (ab 140 m, alle 180–320 m): Abgrund über alle Fahrbahnen, nur die
  mittlere hat eine Insel (6 m, bei hohem Tempo 9 m) – rüberspringen, auf der Insel landen, weiterspringen.
- **Deko-Hindernisse** stehen jetzt ~3 s vor der Katze still im Weg.
  Neue Hindernis-Bilder folgen: in `AObstacle::Build` eintragen.
- **Tintentropfen**: kurzer Spritzer kleiner schwarzer Tropfen beim Absprung, kleiner bei der Landung (`AInkDrops`).
- **Sounds**: `jump` (Sprung), `change lane` (Spurwechsel), `Item Pickup` (?-Box), Musik jetzt 4 Tracks.
  Packaging: `Movies` wird als lose Dateien mitgeliefert (DefaultGame.ini).
- **Sounds**: `Collecting Coin` (Tonhöhe steigt in einer Münzreihe), `Ink Bomb activate` (Tintenbombe).
- **TUTORIAL**: der Rundkurs mit 3 × 3 Fahrbahnen (unverändert), Ziel: alles einfärben, Bestzeit.
- **9 Leben** in beiden Modi: Treffer = Stolpern, kurz langsamer, 2 s blinkend unverwundbar, Tinte bleibt. Anzeige: 9 Punkte unter der Zeit.
- **Grafik**: monochromes Cel Shading als Post-Process `PP_CelMono` (Helligkeitsstufen + Konturen aus der Tiefe, iPhone-tauglich),
  Bäume/Felsen/Hecken aus ChibiArena (`/Game/Nature`), glatte Kurven (Kacheln ~1 m, Wertung weiter in 3-m-Abschnitten).
- **Musik & Sound**: `Tools/Import/Music/*.wav` → MUS_01, MUS_02, … (Reihenfolge nach Dateiname) laufen nacheinander und
  danach wieder von vorn, durchgehend auch zwischen den Läufen. `Tools/Import/Soundeffects/`: Footsteps = Schleife, solange die
  Katze am Boden rennt (Tonhöhe folgt dem Tempo), Ink splash impact = Aufschlag des Riesen-Wurfs und Tintenbombe.
  Neue Dateien einfach dazulegen und `CAT_SETUP=audio` ausführen (24-Bit/Extensible-WAVs werden automatisch gewandelt).
  Lautstärken: `ACatAudio` (MusicVolume 0.45, FootstepVolume 0.35, SplashVolume 0.8). Testschalter `-CatMusicSkip=5`, `-CatNoMusic`.
- Riese austauschen: Dateien nach `Tools/Import/Boss/` (siehe LIESMICH dort), dann `CAT_SETUP=boss`.

## Spielprinzip (Tinte)

Drei ineinanderliegende Rundkurse (Ovale) mit je drei Fahrbahnen = 9 Fahrbahnen. Die Katze läuft automatisch und
hinterlässt eine schwarze Tintenspur auf der Fahrbahn, auf der sie gerade ist. Eingefärbtes bleibt schwarz, auch in
weiteren Runden. Ziel: alle 9 Fahrbahnen komplett schwarz → Erfolgsbildschirm mit Zeit (Bestzeit wird gespeichert).
Hindernisse und schwebende Würfel-Gegner bleiben; ein Treffer beendet den Lauf (Einfärbung dieses Laufs ist weg).

- **Einfärbung**: jede Fahrbahn ist in feste Abschnitte (~3 m) geteilt; ein Abschnitt zählt, sobald Spur oder Item ihn erreicht.
  Optisch wächst die Tinte stufenlos unter der Katze mit. Weiße Stellen leuchten leicht und „atmen“; fehlen einer Fahrbahn
  nur noch ≤ 8 Abschnitte, stehen über jeder Lücke Leuchtsäulen (von weitem sichtbar).
- **Kurswechsel**: nur an den Verbindungsstellen (Mitte beider Geraden, Holzstege mit Leuchtrand und Rauten):
  von der äußersten/innersten Fahrbahn weiter nach außen/innen wischen. Jeder Kurs ist so immer ohne Item erreichbar.
- **Fragezeichen-Boxen**: 2 pro Kurs, kommen 15 s nach dem Einsammeln zurück. Kurze Slot-Machine-Auslosung → Item landet im
  Slot (runder Button unten rechts) und wird erst durch Antippen eingesetzt. **Slot voll**: Boxen bleiben liegen (Hinweis
  „SLOT VOLL“) – ein aufgespartes Item geht so nie versehentlich verloren; wer ein neues will, setzt erst das alte ein.
- **Tintenwolke** (5 s): färbt beim Laufen alle drei Fahrbahnen des aktuellen Kurses. **Tintenbombe**: färbt sofort ±4,8 m
  um die Katze auf allen drei Fahrbahnen – ideal für eine gezielt angesteuerte Lücke.
- **HUD**: Score, Zeit, Gesamt-%, je Kurs (K1–K3) drei Balken L/M/R (weiß = fehlt, schwarz = gefärbt), aktueller Kurs markiert.

Steuerung: Wisch links/rechts = Fahrbahn, hoch = Sprung (Salto, 230 cm hoch), runter = schnell landen, Item-Button antippen.
Im Editor: Pfeile/A-D, Pfeil hoch/W/Leertaste, Pfeil runter/S, E oder Shift = Item, Maus-Ziehen wirkt wie Wischen, F1 = Leistungsanzeige.

## Aufbau (C++, `Source/ShadowCat`)

| Bereich | Klasse / Datei | Aufgabe |
|---|---|---|
| Geometrie | `FCircuitLayout` (CircuitLayout) | Stadion-Ovale, ein Parameter A + seitliche Lage; Fahrbahn-Bogenlängen, Abschnitte, Verbindungsstellen |
| Einfärbung | `AInkCanvas` | alle Abschnitte als ein instanziertes Mesh (1 Draw Call), Wertung, Fortschritt, Leuchtsäulen |
| Kulisse | `ALevelScenery` | Unterbau, Randsteine, Absperrungen, Verbindungsstege, Friedhofs-Deko (instanziert), Nebelschwaden |
| Level-Regeln | `ATrackDirector` (BP_TrackDirector) | Spur einfärben, Kurswechsel, Boxen, Hindernis-Planung je Kurs, Kollision, Tempo, Test-Autopilot |
| Fairness | `FRunPlanner` | Hindernisreihen mit garantiert erreichbarer freier Fahrbahn (jetzt entlang des aktuellen Kurses) |
| Spurwahl | `ULaneMovementComponent` | 9 globale Fahrbahnen, Kurswechsel nur mit Freigabe |
| Items | `UItemSlotComponent`, `AItemBox`, `UBuff_InkCloud`, `UBuff_InkBomb` | Slot + Auslosung, ?-Box, Tinten-Items |
| Spielfigur | `ARunnerCat` (BP_RunnerCat) | Bewegung auf dem Kurs, Sprung, Animationen |
| Ablauf/UI | `ACatRunGameMode`, `UCatRunWidget`, `ACatRunPlayerController`, `ARunCamera`, `UScoreKeeper` | Phasen inkl. Erfolg, HUD, Eingabe, Kamera, Bestzeit |

Neues Item: Unterklasse von `UCatBuff` + Eintrag in `BP_TrackDirector → Items` (Gewicht). Die alten Effekte
(Nebelwolke, Shuriken, Unbesiegbar) sind noch im Code und können dort als Box-Inhalt eingetragen werden.
Levelgröße, Anzahl Kurse/Fahrbahnen, Abschnittslänge, Verbindungsbereich: `BP_TrackDirector → Level → Layout`.

## Assets

`/Game/Cat/*` (Katze + Clips), `/Game/Fx/T_FogPuff` (Nebel-Atlas), `/Game/Materials/`: `M_Cat` (tiefschwarz, Augen sichtbar,
Mond-Glanzlichter), `M_Ink` (Fahrbahn weiß → Tinte), `M_Beacon`, `M_FogPuff`, `M_Mono`, `M_Glow`, `M_Soft`, `M_Disc`.
Blueprints `BP_RunnerCat`, `BP_TrackDirector`. Alles erzeugt `Tools/ue_setup.py` (Editor geschlossen, Pfad mit `/`):
```
UnrealEditor-Cmd.exe ShadowCat.uproject -run=pythonscript -script=C:/Users/johan/Documents/Unreal Projects/ShadowCat/Tools/ue_setup.py -unattended -nosplash -nullrhi
```
Schritte per `CAT_SETUP` (cat, fx, materials, map, blueprints). Hilfsskripte (Blender): `Tools/blender_prepare_cat.py`
(Clips auf der Stelle backen, siehe unten), `Tools/blender_make_fog.py` (Nebeltextur).

## Katze und Animationen

Quellen in `Tools/Import/Cat/Source/` (AccuRig-Exporte). `blender.exe -b --factory-startup --python Tools/blender_prepare_cat.py`
backt alle Clips mit 60 fps **auf der Stelle** (Root fix, Hüfte trägt die Bewegung) → `Tools/Import/Cat/SK_BlackCat.fbx`, `A_*.fbx`;
danach `CAT_SETUP=cat,materials`. Zuordnung in `CLIPS`: A_CatRun, A_CatRunStart, A_CatRunStop, A_CatIdle (Startbildschirm),
A_CatRoll (Sprung/Salto); A_CatJump (flying idle) und A_CatJumpIdle ungenutzt; crawling fehlt (nur .json).
Im Editor austauschbar über `BP_RunnerCat → Katze`.

## Tests (Kommandozeile)

`-CatAuto` (Autopilot: weicht aus, färbt, wechselt Kurse), `-CatScenario=bombgap` (Lücke lassen → Bombe aufsparen → nächste Runde
schließen → Levelende), `-CatNoHazards`, `-CatShot=a.png@Sek`, `-CatWinShot=`, `-CatGameOverShot=`, `-CatJumpEvery=`,
`-CatSideCam`, `-CatNoFog`, `-CatQuit=`. Testläufe schreiben nicht in den Spielstand.

## iPhone-Test: was noch fehlt

1. Mac mit Xcode (oder Remote-Build von Windows auf einen Mac).
2. Apple-ID / Developer-Konto, Bundle-ID `com.johanna.inkrush`, Signing in den iOS-Projekteinstellungen.
3. iPhone per Kabel, Entwicklermodus an, *Package Project* bzw. *Quick Launch*.
4. Messen: Startbildschirm „LEISTUNG: AN“ (FPS, Game/Draw/GPU-ms, alle 5 s auch im Log), „GRAFIK“ schaltet 65/85/100 %
   Auflösung und halbe/volle Deko+Nebel; Grundauflösung `r.MobileContentScaleFactor` in `Config/DefaultDeviceProfiles.ini`.
   Größte Posten fürs iPhone: ~70 transparente Nebel-Quads (Überzeichnung) – bei Bedarf über die Grafikstufe reduzieren.
