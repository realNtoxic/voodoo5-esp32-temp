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

// Schreibt z. B. "Heap 142k 45Hz" nach out (immer nullterminiert,
// notfalls abgeschnitten).
void formatSelfDiagLine(char* out, size_t outSize, uint32_t freeHeapBytes,
                         uint16_t loopHz);
