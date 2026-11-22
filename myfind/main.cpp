#include "search.h"

// C++ Standardbibliotheken
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// POSIX-Funktionen für Prozesse, Speicher und Synchronisation
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

// Synchronisation: Alle Kindprozesse verwenden dieselbe Semaphore im gemeinsam genutzten Speicher. 
//Vor jeder Ergebniszeile wird sie gesperrt und danach freigegeben. 
//Dadurch vermischen sich die Ausgaben verschiedener Kindprozesse nicht.

int main(int argc, char *argv[])
{

  bool recursive = false;
  bool ignoreCase = false;

  // Optionen mit getopt auswerten.
  // Ein vorangestelltes '-' erlaubt auch Optionen nach Dateinamen.
  int option;

  while ((option = getopt(argc, argv, "-:Ri")) != -1)
  {

    switch (option)
    {

    case 'R':
      recursive = true;
      break;

    case 'i':
      ignoreCase = true;
      break;

    case 1:
      // Normales Argument, wird später verarbeitet
      break;

    default:
      std::cerr << "Fehler: Unbekannte Option\n";
      return 1;
    }
  }

  // Positionsargumente unabhängig von der Position
  // der Optionen sammeln.
  std::vector<std::string> arguments;

  // Schleife fängt bei 1 an und ignoriert den Programmnamen
  for (int i = 1; i < argc; ++i)
  {

    std::string arg = argv[i];

    if (arg == "-R" || arg == "-i")
    {
      continue;
    }

    arguments.push_back(arg);
  }

  if (arguments.size() < 2)
  {
    std::cerr << "Usage: ./myfind [-R] [-i] "
              << "searchpath filename1 [filename2 ...]\n";
    return 1;
  }

  // Das erste Positionsargument enthält den Ordner, in dem gesucht wird.
  std::string searchPath = arguments[0];

  // Gemeinsamen Speicher für die Semaphore anlegen.
  // Alle Kindprozesse können dadurch dieselbe Semaphore verwenden.
  void *memory = mmap(
      nullptr,
      sizeof(sem_t),
      PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_ANONYMOUS,
      -1,
      0);

  if (memory == MAP_FAILED)
  {
    // Ohne gemeinsamen Speicher können die Prozesse nicht synchronisiert werden.
    std::cerr << "Fehler bei mmap\n";
    return 1;
  }

  // Die Speicheradresse wird als Zeiger auf eine Semaphore verwendet.
  sem_t *outputLock = static_cast<sem_t *>(memory);

  // Die Semaphore wird für Prozesse freigegeben und startet mit dem Wert 1.
  if (sem_init(outputLock, 1, 1) == -1)
  {
    // Bei einem Fehler wird der zuvor reservierte Speicher wieder freigegeben.
    std::cerr << "Fehler bei sem_init\n";
    munmap(memory, sizeof(sem_t));
    return 1;
  }

  std::vector<pid_t> children;
  bool error = false;

  // Für jeden gesuchten Dateinamen wird ein eigener Kindprozess gestartet.
  // Der erste Eintrag ist der Suchpfad und wird deshalb übersprungen.
  for (size_t i = 1; i < arguments.size(); ++i)
  {

    // fork() teilt den aktuellen Prozess in einen Eltern- und einen Kindprozess.
    pid_t pid = fork();

    if (pid < 0)
    {

      // das negativer Rückgabewert bedeutet, dass der Prozess nicht erstellt wurde.
      std::cerr << "Fehler bei fork: "
                << std::strerror(errno) << "\n";
      error = true;
      break;
    }
    else if (pid == 0)
    {

      // Im Kindprozess wird nach dem aktuellen Dateinamen gesucht.
      bool success = searchFiles(
          searchPath,
          arguments[i],
          recursive,
          ignoreCase,
          outputLock);

      // Der Kindprozess beendet sich mit 0 bei Erfolg und 1 bei einem Fehler.
      _exit(success ? 0 : 1);
    }
    else
    {

      // Der Elternprozess speichert die Prozess-ID für die spätere Kontrolle.
      children.push_back(pid);
    }
  }

  // Auf alle erfolgreich gestarteten Kindprozesse warten
  for (pid_t child : children)
  {

    int status;
    pid_t result;

    // Auf den Kindprozess warten.
    // Falls waitpid durch ein Signal unterbrochen wird (EINTR),
    // wird der Aufruf wiederholt.
    do
    {
      result = waitpid(child, &status, 0);
    } while (result == -1 && errno == EINTR);

    // Bei einem Fehler zum nächsten Kindprozess wechseln,
    // da status in diesem Fall nicht gültig ist.
    if (result == -1)
    {
      std::cerr << "Fehler bei waitpid\n";
      error = true;
      continue;
    }

    // Prüfen, ob der Kindprozess normal und mit Exit-Code 0 beendet wurde.
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
      error = true;
    }
  }

  sem_destroy(outputLock);
  munmap(memory, sizeof(sem_t));

  return error ? 1 : 0;
}