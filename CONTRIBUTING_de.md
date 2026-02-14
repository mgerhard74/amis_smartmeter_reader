# Contributing Guide

## Ziel
Dieses Dokument beschreibt den bevorzugten Entwicklungs- und Release-Ablauf für `amis_smartmeter_reader`.

## Branch-Strategie
Wir verwenden ein leichtes GitFlow-Modell:

- `main`: Stabiler, produktionsreifer Stand. Releases werden hier getaggt.
- `develop`: Integrations-Branch für fertige Features.
- `feature/<kurzer-name>`: Entwicklung einzelner Features oder Refactorings.
- `release/<version>`: Release-Stabilisierung, z. B. `release/1.8.0`.
- `hotfix/<version>`: Kritische Fixes direkt von `main`, z. B. `hotfix/1.8.1`.

## Workflow

### 1) Feature entwickeln
1. Branch von `develop` erstellen: `feature/<name>`.
2. Änderungen in kleinen, nachvollziehbaren Commits einchecken.
3. Pull Reqüst nach `develop` öffnen.
4. Nach Review und grünem CI mergen.

### 2) Release vorbereiten
1. Wenn `develop` stabil ist, `release/<version>` von `develop` abzweigen.
2. Im Release-Branch nur noch:
   - Bugfixes
   - Dokumentation
   - Versionsanpassungen
3. Pull Request von `release/<version>` nach `main`.
4. Nach Merge auf `main` Tag setzen: `v<version>` (z. B. `v1.8.0`).
5. Release-Branch zurück nach `develop` mergen (damit Release-Fixes enthalten sind).

### 3) Hotfix
1. Kritischen Fehler von `main` in `hotfix/<version>` beheben.
2. PR nach `main`, danach Tag `v<version>` setzen.
3. Den Hotfix anschliessend auch nach `develop` mergen.

## Pull Requests
- Ein PR sollte ein klar abgegrenztes Thema haben.
- Titel kurz und präzise formulieren.
- Kurz beschreiben:
  - Was wurde geändert?
  - Warum war die Änderung notwendig?
  - Welche Risiken oder Nebenwirkungen gibt es?
- Wenn relevant: Screenshot/Webinterface-Ansicht oder Testhinweise beilegen.

## Commit-Richtlinien
- Präferiert: kurze, imperative Commit-Messages.
- Beispiele:
  - `Fix wifi reconnect after AP timeout`
  - `Refactor meter parser error handling`
  - `Add MQTT publish guard for invalid values`

## Lokale Checks vor PR
Bitte vor Pull Request lokal ausführen:

```bash
pio run -e esp12e
pio run -e esp12e_debug
cpplint --repository=. --recursive '--extensions=c,cpp,c++,cxx,inl' '--headers=h,hpp,h++,hxx' --filter=-build/c++11,-runtime/references,-readability/braces,-whitespace,-legal,-build/include,-readability/casting ./src ./include
```

## CI/CD Hinweise
- Builds und Lint laufen auf Push und Pull Request (Markdown/Docs-änderungen sind ausgenommen).
- Ein GitHub Release wird erstellt, wenn auf `main` ein Tag im Format `vX.Y.Z` gepusht wird.
- Der Release-Tag soll immer den Stand auf `main` markieren.

## Merge-Empfehlung
- Feature-PRs nach `develop`: bevorzugt Squash-Merge (saubere Historie).
- Release-/Hotfix-PRs: normaler Merge ist ok, damit Kontext erhalten bleibt.

## Schutzregeln (empfohlen)
Für `main` und `develop` in GitHub Branch Protection aktivieren:

- Pull Request verpflichtend
- Mindestens 1 Review
- Erfolgreiche Status Checks verpflichtend
- Direktes Pushen auf geschützte Branches verbieten