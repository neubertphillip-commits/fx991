#include "store.h"

#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <algorithm>

#include "crc32.h"

#ifdef ESP_PLATFORM
#include <LittleFS.h>
#endif

namespace {

#ifdef ESP_PLATFORM
const char* root = "/littlefs";  // Einhaengepunkt von LittleFS im VFS (fopen & Co.)
#else
const char* root = "sim-files";
constexpr uint32_t SIM_CAPACITY = 0x180000;  // so gross wie die Flash-Partition
#endif
bool mounted = false;

constexpr const char* PART = ".part";  // Zwischendatei beim Empfang
FILE* wfile = nullptr;
char wname[store::NAME_LEN + 1];
uint32_t wleft = 0;

bool makePath(char* out, size_t cap, const char* name) {
  int n = snprintf(out, cap, "%s/%s", root, name);
  return n > 0 && static_cast<size_t>(n) < cap;
}

constexpr size_t PATH_LEN = 256;

}  // namespace

namespace store {

void setRoot(const char* dir) { root = dir; }

bool begin() {
#ifdef ESP_PLATFORM
  mounted = LittleFS.begin(true);  // true: beim allerersten Start formatieren
#else
  mkdir(root, 0755);
  struct stat st;
  mounted = stat(root, &st) == 0 && S_ISDIR(st.st_mode);
#endif
  if (mounted) {
    char p[PATH_LEN];
    if (makePath(p, sizeof(p), PART)) ::remove(p);  // Rest eines abgebrochenen Empfangs
  }
  return mounted;
}

bool ready() { return mounted; }

bool validName(const char* name) {
  size_t len = strlen(name);
  if (len == 0 || len > NAME_LEN || name[0] == '.') return false;
  for (size_t i = 0; i < len; i++) {
    char c = name[i];
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

bool isImage(const char* name) {
  size_t len = strlen(name);
  return (len > 4 && !strcasecmp(name + len - 4, ".jpg")) ||
         (len > 5 && !strcasecmp(name + len - 5, ".jpeg"));
}

std::vector<Entry> list() {
  std::vector<Entry> out;
  if (!mounted) return out;
  DIR* dir = opendir(root);
  if (!dir) return out;
  while (dirent* de = readdir(dir)) {
    if (!validName(de->d_name)) continue;  // auch ".", ".." und die Zwischendatei
    char p[PATH_LEN];
    struct stat st;
    if (!makePath(p, sizeof(p), de->d_name) || stat(p, &st) != 0 || !S_ISREG(st.st_mode)) continue;
    Entry e;
    strncpy(e.name, de->d_name, sizeof(e.name) - 1);
    e.name[sizeof(e.name) - 1] = '\0';
    e.size = static_cast<uint32_t>(st.st_size);
    out.push_back(e);
  }
  closedir(dir);
  std::sort(out.begin(), out.end(),
            [](const Entry& a, const Entry& b) { return strcasecmp(a.name, b.name) < 0; });
  return out;
}

FILE* open(const char* name) {
  char p[PATH_LEN];
  if (!mounted || !validName(name) || !makePath(p, sizeof(p), name)) return nullptr;
  return fopen(p, "rb");
}

bool remove(const char* name) {
  char p[PATH_LEN];
  if (!mounted || !validName(name) || !makePath(p, sizeof(p), name)) return false;
  return ::remove(p) == 0;
}

uint32_t crc(const char* name) {
  FILE* f = open(name);
  if (!f) return 0;
  uint8_t buf[512];
  uint32_t c = 0;
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) c = crc32Update(c, buf, n);
  fclose(f);
  return c;
}

uint32_t freeBytes() {
  if (!mounted) return 0;
#ifdef ESP_PLATFORM
  size_t total = LittleFS.totalBytes();
  size_t used = LittleFS.usedBytes();
  return total > used ? static_cast<uint32_t>(total - used) : 0;
#else
  // grob wie LittleFS: jede Datei belegt ganze 4-kB-Bloecke plus einen fuer Verwaltung
  uint32_t used = 0;
  for (const Entry& e : list()) used += ((e.size + 4095) / 4096 + 1) * 4096;
  return used < SIM_CAPACITY ? SIM_CAPACITY - used : 0;
#endif
}

bool beginWrite(const char* name, uint32_t size) {
  abortWrite();
  char p[PATH_LEN];
  if (!mounted || !validName(name) || !makePath(p, sizeof(p), PART)) return false;
  remove(name);
  wfile = fopen(p, "wb");
  if (!wfile) return false;
  strcpy(wname, name);
  wleft = size;
  if (size == 0) return write(nullptr, 0);
  return true;
}

bool write(const uint8_t* data, size_t len) {
  if (!wfile) return false;
  if (len > wleft || (len && fwrite(data, 1, len, wfile) != len)) {
    abortWrite();
    return false;
  }
  wleft -= static_cast<uint32_t>(len);
  if (wleft > 0) return true;
  // komplett: Zwischendatei umbenennen
  bool ok = fclose(wfile) == 0;
  wfile = nullptr;
  char from[PATH_LEN], to[PATH_LEN];
  ok = ok && makePath(from, sizeof(from), PART) && makePath(to, sizeof(to), wname) &&
       rename(from, to) == 0;
  if (!ok && makePath(from, sizeof(from), PART)) ::remove(from);
  return ok;
}

bool writing() { return wfile != nullptr; }

void abortWrite() {
  if (!wfile) return;
  fclose(wfile);
  wfile = nullptr;
  char p[PATH_LEN];
  if (makePath(p, sizeof(p), PART)) ::remove(p);
}

}  // namespace store
