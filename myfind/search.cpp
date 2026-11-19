#include "search.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <string>

#include <unistd.h>
#include <semaphore.h>

namespace fs = std::filesystem;

// Wandelt einen String in Kleinbuchstaben um.
std::string toLower(std::string text)
{

  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c)
                 {
                   return static_cast<char>(std::tolower(c));
                 });

  return text;
}

// Vergleicht zwei Dateinamen.
// Optional wird Gross-/Kleinschreibung ignoriert.
bool matches(const std::string &actual,
             const std::string &wanted,
             bool ignoreCase)
{

  if (ignoreCase)
  {
    return toLower(actual) == toLower(wanted);
  }

  return actual == wanted;
}

// Gibt einen gefundenen Dateipfad aus.
// Eine Semaphore verhindert, dass mehrere Kindprozesse
// gleichzeitig auf stdout schreiben.
bool printResult(const std::string &filename,
                 const fs::path &path,
                 sem_t *outputLock)
{

  // Ausgabezeile vorbereiten:
  // <pid>: <filename>: <absolute-path>
  std::string line =
      std::to_string(getpid()) + ": " +
      filename + ": " +
      fs::absolute(path).lexically_normal().string() + "\n";

  // Semaphore sperren.
  while (sem_wait(outputLock) == -1)
  {

    if (errno != EINTR)
    {
      return false;
    }
  }

  bool success = true;
  size_t offset = 0;

  // Gesamte Zeile schreiben.
  // write() kann weniger Bytes als angefordert schreiben.
  while (offset < line.size())
  {

    ssize_t written = write(
        STDOUT_FILENO,
        line.data() + offset,
        line.size() - offset);

    if (written < 0 && errno == EINTR)
    {
      continue;
    }

    if (written <= 0)
    {
      success = false;
      break;
    }

    offset += static_cast<size_t>(written);
  }

  // Semaphore wieder freigeben.
  if (sem_post(outputLock) == -1)
  {
    return false;
  }

  return success;
}

// Sucht nach einem Dateinamen innerhalb eines Verzeichnisses.
bool searchFiles(const std::string &searchPath,
                 const std::string &filename,
                 bool recursive,
                 bool ignoreCase,
                 sem_t *outputLock)
{

  try
  {

    fs::path directory(searchPath);

    // Pruefen, ob das Verzeichnis existiert.
    if (!fs::exists(directory) ||
        !fs::is_directory(directory))
    {

      std::cerr << "Fehler: Ungueltiges Verzeichnis: "
                << searchPath << "\n";

      return false;
    }

    if (recursive)
    {

      // Rekursive Suche inklusive Unterverzeichnissen.
      for (const auto &entry :
           fs::recursive_directory_iterator(directory))
      {

        if (entry.is_regular_file() &&
            matches(entry.path().filename().string(),
                    filename,
                    ignoreCase))
        {

          if (!printResult(filename,
                           entry.path(),
                           outputLock))
          {

            std::cerr << "Fehler bei der Ausgabe\n";
            return false;
          }
        }
      }
    }
    else
    {

      // Suche nur im angegebenen Verzeichnis.
      for (const auto &entry :
           fs::directory_iterator(directory))
      {

        if (entry.is_regular_file() &&
            matches(entry.path().filename().string(),
                    filename,
                    ignoreCase))
        {

          if (!printResult(filename,
                           entry.path(),
                           outputLock))
          {

            std::cerr << "Fehler bei der Ausgabe\n";
            return false;
          }
        }
      }
    }
  }
  catch (const fs::filesystem_error &e)
  {

    std::cerr << "Dateisystemfehler: "
              << e.what() << "\n";

    return false;
  }

  return true;
}