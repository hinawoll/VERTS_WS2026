#ifndef SEARCH_H
#define SEARCH_H

#include <string>
#include <semaphore.h>

// Durchsucht ein Verzeichnis nach einem bestimmten Dateinamen.
// recursive: Unterverzeichnisse ebenfalls durchsuchen.
// ignoreCase: Gross-/Kleinschreibung ignorieren.
// outputLock: Synchronisiert die Ausgabe zwischen Kindprozessen.
//
// Rueckgabe: true bei erfolgreicher Suche, false bei Fehlern.
bool searchFiles(const std::string &searchPath,
                 const std::string &filename,
                 bool recursive,
                 bool ignoreCase,
                 sem_t *outputLock);

#endif