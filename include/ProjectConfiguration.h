#pragma once

// Eckdaten der Applikation
#define APP_NAME        "Amis"
#define APP_VERSION_STR "1.5.8"


// Pin, mit dem der Zähler mittels Jumper auf Masse
// in den Access-Point-Modus (AP-Mode) gesetzt wird
#ifndef AP_PIN
#define AP_PIN      14
#endif


// Die blaue LED (hängt mit Vorwiderstand an VCC)
#ifndef LED_PIN
#define LED_PIN      2
#endif

// Die serielle Schnittstelle an der die IR Dioden sind
// Mit dieser wird mit dem eigentlichen Zähler kommuniziert
// Erlaubt: 0...keine Kommunikation
//          1...Serielle #1  (So wäre die Hardware gebaut -  die IR Dioden hänen da dran)
//          2...Serielle #2  (Weil es die Serielle#2 halt auch gibt)
#ifndef AMISREADER_SERIAL_NO
#define AMISREADER_SERIAL_NO    1
#endif


// Ausgabe aller Meldungen (Debugginghilfe)
// Erlaubt: 0...keine Kommunikation
//          1...Serielle #1 (So wäre die Hardware gebaut, dass die verwendet werden könnte)
//          2...Serielle #2 (Weil es die Serielle#2 halt auch gibt)
#ifndef MSGOUT_SERIAL_NO
#define MSGOUT_SERIAL_NO        0
#endif


/* vim:set ts=4 et: */
