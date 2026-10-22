# Bedienungsanleitung – Dienstplan-Terminal

**Entwurf – noch nicht für den produktiven Betrieb freigegeben**  
**Sprache:** Deutsch  
**Geltungsbereich:** vollständig implementiertes, fertig montiertes Dienstplan-Terminal gemäß Roadmap

> Diese Anleitung beschreibt das geplante fertige Terminal. Sie gilt nicht für den aktuellen Hardware-Teststand. Kapitel mit dem Vermerk **Offen** müssen vor einer Freigabe ergänzt und mit dem fertigen Gerät abgeglichen werden.

## Inhalt

1. [Zweck und Zuständigkeiten](#1-zweck-und-zuständigkeiten)
2. [Das Terminal im Überblick](#2-das-terminal-im-überblick)
3. [Inbetriebnahme und Provisioning](#3-inbetriebnahme-und-provisioning)
4. [Im Alltag buchen](#4-im-alltag-buchen)
5. [Anzeigen und Rückmeldungen](#5-anzeigen-und-rückmeldungen)
6. [Fehler beheben](#6-fehler-beheben)
7. [Wartung, Updates und Außerbetriebnahme](#7-wartung-updates-und-außerbetriebnahme)
8. [Kontakt und Projektinformationen](#8-kontakt-und-projektinformationen)

## 1. Zweck und Zuständigkeiten

Das Dienstplan-Terminal dient dazu, mit einer zugeordneten NFC-Karte Dienstbeginn, Dienstende, Pausenbeginn und Pausenende zu buchen. Der Dienstplan-Server entscheidet, welcher Person eine Karte zugeordnet ist, welcher Buchungszustand vorliegt, welche Aktionen zulässig sind und ob eine Buchung erfolgreich war.

Das Terminal nimmt Buchungen nur mit erreichbarem WLAN und Server an. Es speichert keine Buchungen für eine spätere Übertragung zwischen.

### Zuständigkeiten

- **Mitarbeitende** scannen eine zugeordnete Karte und wählen eine vom Terminal freigegebene Aktion.
- **Administratoren** richten das Terminal ein und provisionieren es. Dafür ist ein Admin-Zugang zum Dienstplan-Server erforderlich. Der konkrete Provisioning-Ablauf ist noch offen.
- **Mitarbeitende oder Administratoren** ordnen NFC-Karten über die Android-App zu. Die Zuordnung oder Verwaltung von Karten erfolgt nicht am Terminal.
- **Manuelle Korrekturen und Nacharbeiten** erfolgen direkt im Dienstplan-Server oder in der Android-App.

Eine Person kann mehrere NFC-Karten besitzen, zum Beispiel einen Dongle und ein NFC-fähiges Smartphone sowie weitere Geräte. Eine Karte kann jeweils einer Person zugeordnet sein. Eine unbekannte Karte kann über die Android-App zugeordnet werden. Wird eine wiedergefundene Karte erneut zugeordnet, kann sie wieder verwendet werden. Eine separate Sperrliste gibt es nicht.

## 2. Das Terminal im Überblick

Das Terminal besteht im Betrieb aus einem Display, einem NFC-Lesefeld, vier Aktionstasten und optischen beziehungsweise akustischen Rückmeldungen. Je nach Bauausführung befindet sich die LED direkt in einer Taste oder neben ihr.

Die vier Aktionen sind:

- **Dienstbeginn**
- **Dienstende**
- **Pausenbeginn**
- **Pausenende**

Eine Taste ist nur dann auswählbar, wenn ihre LED leuchtet. Die zulässigen Aktionen hängen vom aktuellen, vom Server gelieferten Zustand ab:

| Zustand laut Server | Freigegebene Aktion(en) |
|---|---|
| Abwesend | Dienstbeginn |
| Im Dienst | Dienstende oder Pausenbeginn |
| In der Pause | Pausenende |

## 3. Inbetriebnahme und Provisioning

### 3.1 Erststart

Beim Start prüft das Terminal, ob WLAN-Zugangsdaten eingerichtet sind.

- Sind keine WLAN-Zugangsdaten vorhanden, startet das Terminal den Einrichtungsmodus über einen WLAN-Zugangspunkt (SoftAP).
- Sind WLAN-Zugangsdaten vorhanden, startet das Terminal in den Betriebszustand **Warten auf Karte**.

Beim Neustart beginnt das Terminal immer im Zustand **Warten auf Karte** – außer, es fehlen WLAN-Zugangsdaten; dann startet es den SoftAP für das Provisioning. Ein vorheriger Buchungszustand wird nicht lokal fortgesetzt.

### 3.2 SoftAP und Einrichtung

Das Terminal erzeugt für den SoftAP ein zufälliges WPA2-Passwort und zeigt es auf dem Display an.

**Offen – vor Freigabe ergänzen:**

- Name des WLAN-Zugangspunkts und Anzeigeformat des Passworts
- Wie lange der Zugangspunkt aktiv bleibt und wie er beendet wird
- Wie Administratoren die Einrichtungsseite aufrufen
- Welche Felder und Prüfschritte die Einrichtungsseite enthält
- Wie WLAN, API-Adresse und Filiale ausgewählt und gespeichert werden
- Wie das Gerät serverseitig angelegt und seine Gerätezugangsdaten provisioniert werden
- Welche Bestätigung nach erfolgreicher Einrichtung erscheint
- Vorgehen bei falschem Passwort, fehlender Verbindung oder abgebrochener Einrichtung

### 3.3 Provisioning durch Administratoren

Die Einrichtung kann von jeder Person mit einem Admin-Zugang zum Dienstplan-Server vorgenommen werden. Zum Provisioning gehören mindestens die WLAN-Verbindung, die API-Adresse, die Filialzuordnung und die gerätespezifische Identität beziehungsweise die Zugangsdaten für den Dienstplan-Server.

**Offen – der konkrete Ablauf folgt.** Vor Freigabe dieser Anleitung muss beschrieben werden, wie ein Terminal angelegt wird, wie Gerätezugangsdaten sicher auf das Gerät gelangen und wie Zugangsdaten erneuert oder ein Gerät deaktiviert wird. Zugangsdaten dürfen nicht öffentlich dokumentiert oder in Supportanfragen mitgeschickt werden.

## 4. Im Alltag buchen

### 4.1 Ruhezustand

Im Ruhezustand zeigt das Display die Uhrzeit und die Aufforderung, eine Karte zu scannen. Nach einem Neustart startet das Terminal wieder in diesem Zustand, sofern WLAN-Zugangsdaten vorhanden sind.

### 4.2 Karte scannen

Halten Sie eine Ihnen zugeordnete NFC-Karte oder ein NFC-fähiges Gerät an das NFC-Lesefeld. Die Kartenlesung gilt als abgeschlossen, sobald das Terminal die Kartenkennung gelesen hat. Nehmen Sie die Karte anschließend vom Lesefeld.

Nach dem Lesen fragt das Terminal den Dienstplan-Server ab. Das Display zeigt die Rückmeldung der API. Bei einer zugeordneten Karte werden die Person und die aktuell zulässigen Aktionen angezeigt. Die LEDs kennzeichnen die auswählbaren Aktionstasten.

Die Zehn-Sekunden-Frist beginnt beim Lesen der Karte. Innerhalb dieser Frist kann dieselbe Karte nicht erneut gelesen werden. Eine andere Karte kann die zuvor gelesene Karte ersetzen – auch innerhalb des Zehn-Sekunden-Fensters.

### 4.3 Aktion auswählen

Drücken Sie eine Taste, deren LED leuchtet. Nicht beleuchtete Tasten sind für den angezeigten Zustand nicht freigegeben.

Die Auswahl kann am Terminal nicht zurückgenommen werden. Nach dem Tastendruck sendet das Terminal die Buchungsanfrage an den Server. Der Server prüft die Aktion erneut; eine zuvor beleuchtete Taste garantiert daher noch keinen Buchungserfolg.

### 4.4 Ergebnis abwarten

Das Terminal zeigt das vom Server zurückgegebene Buchungsergebnis und gibt die dazugehörige Rückmeldung aus. Das Ergebnis bleibt zehn Sekunden sichtbar.

Nach einem Fehler, der eine neue Benutzeraktion erfordert, kann die Karte sofort erneut gelesen werden. Ob ein erneuter Versuch sinnvoll ist, hängt vom Fehler ab: Bei einem vorübergehenden Fehler kann ein neuer Versuch genügen. Bei einem dauerhaften Fehler muss das Gerät gegebenenfalls erst repariert oder durch eine zuständige Person geprüft werden.

Wenn eine Buchung manuell korrigiert oder nachgetragen werden muss, verwenden Sie dafür den Dienstplan-Server oder die Android-App. Das Terminal bietet keine Funktion für manuelle Nacharbeiten.

### 4.5 Unbekannte Karte

Ist eine Karte keiner Person zugeordnet, kann sie am Terminal nicht registriert werden. Ordnen Sie die Karte über die Android-App zu und scannen Sie sie danach erneut.

Eine wiedergefundene Karte kann erneut einer Person zugeordnet werden. Eine zusätzliche Sperrlistenverwaltung am Terminal ist nicht erforderlich.

### 4.6 Ablauf ohne Tastendruck

Nach dem Lesen einer Karte gilt eine Frist von zehn Sekunden. Wird innerhalb dieses Zeitraums keine Aktion gewählt, endet die Auswahl. Für einen weiteren Versuch muss die Karte erneut gelesen werden.

**Hinweis:** Ob zehn Sekunden für alle Mitarbeitenden und in allen Situationen ausreichen, wird in Praxistests mit dem ersten fertig implementierten Prototyp geprüft.

## 5. Anzeigen und Rückmeldungen

Die genaue Zuordnung von Bildschirmmeldungen, LED-Farben und Tonsignalen wird nach Implementierung ergänzt.

| Situation | Erwartetes Verhalten laut bisheriger Festlegung |
|---|---|
| Ruhezustand | Uhrzeit und Aufforderung, eine Karte zu scannen |
| Karte erkannt und API-Antwort erhalten | Anzeige gemäß API-Antwort; zulässige Aktionstasten sind durch LEDs markiert |
| WLAN oder Server nicht erreichbar | Fehlerton und Fehlermeldung auf dem Display; keine lokale Buchung |
| Buchungsantwort | Erfolg oder Fehler gemäß Serverantwort; Ergebnis zehn Sekunden sichtbar |
| SoftAP zur Einrichtung | Zufälliges WPA2-Passwort auf dem Display |

**Offen – nach Implementierung ergänzen:** exakte Texte, Signaltonfolgen, LED-Farben und Blinkmuster sowie die Bedeutung jedes Signals.

## 6. Fehler beheben

### Karte wird nicht erkannt

1. Halten Sie die Karte erneut an das NFC-Lesefeld.
2. Falls gerade eine andere Karte gelesen wurde, warten Sie, bis die Anzeige für diese Karte beendet ist, oder verwenden Sie die andere Karte als Ersatz für die aktuelle Auswahl.
3. Wird die Karte wiederholt nicht erkannt, verwenden Sie – sofern zugeordnet – eine andere Karte oder ein anderes NFC-fähiges Gerät.

### Karte ist unbekannt

Ordnen Sie die Karte in der Android-App einer Person zu. Das Terminal kann die Zuordnung nicht selbst vornehmen. Scannen Sie die Karte nach erfolgreicher Zuordnung erneut.

### WLAN oder Dienstplan-Server nicht erreichbar

Das Terminal zeigt nach dem Scan eine Fehlermeldung und gibt einen Fehlerton aus. Es nimmt keine Buchung offline an und stellt keine lokale Warteschlange bereit. Prüfen Sie später erneut. Wenn eine Fehlermeldung bestehen bleibt, informieren Sie die zuständige Person oder melden Sie das Problem über die Projekt-Issues (siehe [Kontakt und Projektinformationen](#8-kontakt-und-projektinformationen)).

### Buchung wird abgelehnt oder schlägt fehl

Folgen Sie der angezeigten Meldung. Bei einem vorübergehenden Fehler kann die Karte nach der Fehlerrückmeldung sofort erneut gescannt werden. Bei einem dauerhaften Fehler muss das Terminal gegebenenfalls geprüft oder repariert werden. Ist eine manuelle Korrektur erforderlich, verwenden Sie den Dienstplan-Server oder die Android-App.

**Offen – nach Implementierung ergänzen:** konkrete Fehlermeldungen mit Ursachen, Handlungsschritten, Wiederholbarkeit und Eskalationsweg. Insbesondere muss die Anleitung unterscheiden, wann ein erneuter Versuch sicher ist und wann zunächst der Buchungsstatus geprüft werden sollte.

## 7. Wartung, Updates und Außerbetriebnahme

### Firmware-Updates (OTA)

**Offen – Kapitel nach Festlegung und Implementierung des Updateverfahrens ergänzen.** Zu dokumentieren sind Berechtigung, Updatequelle, Start des Updates, erforderliche Verbindung, Erfolgskontrolle und Vorgehen nach einem fehlgeschlagenen Update.

### Statussignale und weitergehende Störungshilfe

**Offen – Kapitel nach Festlegung der endgültigen Displaymeldungen, LED-Signale, Töne und Diagnosemöglichkeiten ergänzen.** Für jede Störung müssen Bedeutung, mögliche Abhilfe und Eskalationsweg angegeben werden.

### Austausch oder Außerbetriebnahme

**Offen – Ablauf ergänzen.** Zu klären sind insbesondere die Deaktivierung beziehungsweise der Widerruf der Gerätezugangsdaten im Dienstplan-Server sowie der Umgang mit gespeicherter WLAN-Konfiguration.

## 8. Kontakt und Projektinformationen

Der Quellcode und die Hardwareunterlagen des Projekts sind Open Source. Fehlerberichte und Verbesserungsvorschläge können über die GitHub-Issues eingereicht werden:

**GitHub-Issues:** <https://github.com/MaMaKow/dienstplan-terminal-esp32/issues>

Bitte in öffentlichen Issues keine Namen von Mitarbeitenden, Kartennummern oder NFC-UIDs, WLAN-Passwörter, Gerätegeheimnisse, Zugriffstoken oder sonstige personenbezogene beziehungsweise vertrauliche Daten veröffentlichen. Für betriebliche Notfälle ist der interne Supportweg der Organisation zu verwenden; GitHub-Issues sind kein Echtzeit-Notfallkanal.

