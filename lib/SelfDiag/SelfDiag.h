// =============================================================
//  SelfDiag.h — Formatiert die freie SelfDiag-Zeile (freier Heap,
//  Loop-Frequenz) fuer DashboardData.selfLine.
//
//  Reine Formatierung, KEIN Arduino.h -> nativ testbar. Das
//  Auslesen von ESP.getFreeHeap()/getMinFreeHeap() passiert in der
//  realen Hal-Implementierung; hier wird nur aus den Rohwerten eine
//  Textzeile gebaut. Komfort wie Logging: faellt die Formatierung
//  aus (z. B. Puffer zu klein), darf das die Regelung nicht
//  beeinflussen -- deshalb reine, seiteneffektfreie Funktion ohne
//  Fehlerpfad, der irgendwo anders durchschlagen koennte.
// =============================================================
#pragma once
#include <cstddef>
#include <cstdint>

// Schreibt z. B. "Heap:142k | Hz:45 *" nach out (immer nullterminiert,
// notfalls abgeschnitten). Format wie das volle Laufband (siehe
// CLAUDE.md "Laufband"): Wertepaare als "Schluessel:Wert", durch " | "
// getrennt; das "*" am Ende markiert die Stelle, an der die Zeile beim
// nahtlosen Umlauf (siehe ScrollLine.h) wieder von vorn beginnt.
void formatSelfDiagLine(char* out, size_t outSize, uint32_t freeHeapBytes,
                         uint16_t loopHz);
