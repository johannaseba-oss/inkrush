# Inkrush – iOS-Build und Deployment

Inkrush ist ein Endless-Runner im Hochformat für das iPhone, gebaut mit **Unreal Engine 5.8** und C++ (Modul `ShadowCat`).
Alle Spiel-Assets liegen bereits importiert in `Content/`. Das Setup-Skript `Tools/ue_setup.py` wird zum Bauen **nicht** gebraucht.

## Voraussetzungen

- Ein Mac mit aktuellem **Xcode**, einmal gestartet, Lizenz akzeptiert, iOS-Plattform installiert
- **Unreal Engine 5.8** auf dem Mac, aus dem Epic Games Launcher und mit iOS-Unterstützung
- **Git LFS**: `brew install git-lfs && git lfs install`
- Ein Apple-Developer-Account mit Team-ID

## 1. Klonen

```bash
git lfs install
git clone <REPO-URL> Inkrush
cd Inkrush
git lfs pull
```

Prüfen: Die `.uasset`-Dateien in `Content/` müssen echte Dateien mit mehreren KB bis MB sein, keine LFS-Zeiger von etwa 130 Byte.

## 2. Projekt öffnen

Öffne `ShadowCat.uproject` mit UE 5.8. Die Frage, ob das Modul `ShadowCat` gebaut werden soll, beantwortest du mit **Ja**. Dafür braucht es Xcode.
Alternativ: Rechtsklick auf die `.uproject` → *Generate Xcode Project*, dann den Workspace in Xcode bauen.

## 3. Signierung einstellen

Gehe zu *Edit → Project Settings → Platforms → iOS*:

- **Bundle Identifier**: steht auf `com.johanna.inkrush`. Für dein eigenes Team musst du ihn eventuell auf eine eigene ID ändern, die im Developer-Portal registriert ist.
- **Signing**: *Automatic Signing* aktivieren und die **IOS Team ID** eintragen. Alternativ Provisioning Profile und Zertifikat manuell importieren.
- Orientierung ist nur Hochformat, Framerate 60 fps. Beides ist schon gesetzt, in `Config/DefaultEngine.ini`.

## 4. Bauen und Paketieren

**Im Editor:** *Platforms → iOS → Package Project* mit Konfiguration **Shipping**. Am Ende liegt eine `.ipa` vor.

**Per Kommandozeile:**

```bash
"<UE_5.8>/Engine/Build/BatchFiles/RunUAT.sh" BuildCookRun \
  -project="$PWD/ShadowCat.uproject" -platform=IOS -clientconfig=Shipping \
  -build -cook -stage -pak -iostore -package -archive -archivedirectory="$PWD/Archive" -nocompileeditor
```

Für einen Test auf dem angeschlossenen iPhone: *Platforms → iOS → <Gerät> → Launch*.

## 5. TestFlight / App Store

1. Erstelle die App in **App Store Connect** mit derselben Bundle-ID.
2. Lade die `.ipa` mit **Transporter** oder dem Xcode-Organizer hoch.
3. Gib den Build in TestFlight frei.

## Hinweise

- **Videos** des Gegners liegen in `Content/Movies/*.mp4`. Sie werden laut `Config/DefaultGame.ini` als lose Dateien mitgeliefert (`DirectoriesToAlwaysStageAsNonUFS`).
- **Spielstand** (Münzen, Shop-Upgrades) wird lokal als SaveGame gespeichert. Es gibt keinen Server.
- **Die Plugins** *PythonScript* und *EditorScriptingUtilities* gelten nur im Editor und kommen nicht in den iOS-Build.
- `Tools/` enthält die Quelldateien (PNG, FBX, WAV, MP4) und Skripte zum Neu-Importieren. Zum Bauen brauchst du das nicht. Details stehen in `Docs/README.md`.
- **Steuerung auf dem iPhone**:
  - Wischen links/rechts: Fahrbahn wechseln
  - Wischen hoch: springen
  - Wischen runter: kriechen
  - Items im Inventar rechts antippen
  - Pause oben links
