#define FUSE_USE_VERSION 31
#include <fuse3/fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>

#define TRAP_STATE_FILE "/var/lib/troll/trap_state"

static const char *spicy_file = "very_spicy_info.txt";
static const char *upload_file = "upload.txt";

// Fungsi: cek apakah jebakan telah aktif
int trap_triggered() {
    FILE *f = fopen(TRAP_STATE_FILE, "r");
    if (!f) return 0;
    int flag = 0;
    fscanf(f, "%d", &flag);
    fclose(f);
    return flag == 1;
}

// Fungsi: aktifkan jebakan
void activate_trap() {
    FILE *f = fopen(TRAP_STATE_FILE, "w");
    if (!f) return;
    fprintf(f, "1\n");
    fclose(f);
}

// Ambil nama user dari UID FUSE context
char *get_username() {
    uid_t uid = fuse_get_context()->uid;
    struct passwd *pw = getpwuid(uid);
    return pw ? pw->pw_name : "unknown";
}

// Fungsi pembantu membandingkan path
int is_path(const char *path, const char *filename) {
    char full[256];
    snprintf(full, sizeof(full), "/%s", filename);
    return strcmp(path, full) == 0;
}

// Implementasi getattr
static int troll_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    (void) fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
    } else if (is_path(path, spicy_file) || is_path(path, upload_file)) {
        stbuf->st_mode = S_IFREG | 0644;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024;
    } else {
        return -ENOENT;
    }

    return 0;
}

// Implementasi readdir (versi FUSE3)
static int troll_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fi;
    (void) flags;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    filler(buf, spicy_file, NULL, 0, 0);
    filler(buf, upload_file, NULL, 0, 0);

    return 0;
}

// Implementasi open
static int troll_open(const char *path, struct fuse_file_info *fi) {
    if (is_path(path, spicy_file) || is_path(path, upload_file)) {
        return 0;
    }
    return -ENOENT;
}

// Implementasi read
static int troll_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    (void) fi;

    char *username = get_username();
    const char *content;

    if (trap_triggered()) {
        content = "Fell for it again reward\n";
    } else if (is_path(path, spicy_file)) {
        if (strcmp(username, "DainTontas") == 0) {
            content = "Very spicy internal developer information: leaked roadmap.docx\n";
        } else {
            content = "DainTontas' personal secret!!.txt\n";
        }
    } else if (is_path(path, upload_file)) {
        content = "";
    } else {
        return -ENOENT;
    }

    size_t len = strlen(content);
    if (offset >= len) return 0;
    if (offset + size > len) size = len - offset;

    memcpy(buf, content + offset, size);
    return size;
}

// Implementasi write
static int troll_write(const char *path, const char *buf, size_t size, off_t offset,
                       struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    if (is_path(path, upload_file)) {
        if (strncmp(buf, "upload", 6) == 0) {
            activate_trap();
        }
        return size;
    }

    return -EACCES;
}

// Daftar operasi
static const struct fuse_operations troll_oper = {
    .getattr = troll_getattr,
    .readdir = troll_readdir,
    .open    = troll_open,
    .read    = troll_read,
    .write   = troll_write,
};

// Main
int main(int argc, char *argv[]) {
    umask(0);
    mkdir("/var/lib/troll", 0755); // Buat direktori trap jika belum ada
    return fuse_main(argc, argv, &troll_oper, NULL);
}

// gcc -Wall `pkg-config fuse3 --cflags` troll_fs.c -o troll_fs `pkg-config fuse3 --libs`
