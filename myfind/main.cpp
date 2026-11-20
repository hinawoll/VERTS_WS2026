#include "search.h"

// C++ Standardbibliotheken
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// POSIX Funktionen fuer Prozesse, Speicher und Synchronisation
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>

/* Synchronisation: Alle Kindprozesse verwenden dieselbe Semaphore im gemeinsam genutzten Speicher. Vor jeder Ergebniszeile wird sie gesperrt und danach freigegeben. Dadurch vermischen sich die Ausgaben verschiedener Kindprozesse nicht.*/

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

  // Schleife faengt bei 1 an und ignoriert den Programmnamen
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

  std::string searchPath = arguments[0];

  // Gemeinsamen Speicher für die Semaphore anlegen.
  void *memory = mmap(
      nullptr,
      sizeof(sem_t),
      PROT_READ | PROT_WRITE,
      MAP_SHARED | MAP_ANONYMOUS,
      -1,
      0);

  if (memory == MAP_FAILED)
  {
    std::cerr << "Fehler bei mmap\n";
    return 1;
  }

  sem_t *outputLock = static_cast<sem_t *>(memory);

  if (sem_init(outputLock, 1, 1) == -1)
  {
    std::cerr << "Fehler bei sem_init\n";
    munmap(memory, sizeof(sem_t));
    return 1;
  }

  std::vector<pid_t> children;
  bool error = false;

  // Fuer jeden Dateinamen einen Kindprozess erzeugen, path wird uebersprungen
  for (size_t i = 1; i < arguments.size(); ++i)
  {

    pid_t pid = fork();

    if (pid < 0)
    {

      std::cerr << "Fehler bei fork: "
                << std::strerror(errno) << "\n";
      error = true;
      break;
    }
    else if (pid == 0)
    {

      // Kindprozess
      bool success = searchFiles(
          searchPath,
          arguments[i],
          recursive,
          ignoreCase,
          outputLock);

      _exit(success ? 0 : 1);
    }
    else
    {

      // Elternprozess
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