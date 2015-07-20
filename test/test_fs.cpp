/* Copyright 2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h> /* unlink, rmdir, etc. */

#include <uv.h>

#include "runner.h"


//-----------------------------------------------------------------------------


typedef struct {
  const char* path;
  double atime;
  double mtime;
} utime_check_t;


static int dummy_cb_count;
static int close_cb_count;
static int create_cb_count;
static int open_cb_count;
static int read_cb_count;
static int write_cb_count;
static int unlink_cb_count;
static int mkdir_cb_count;
static int mkdtemp_cb_count;
static int rmdir_cb_count;
static int scandir_cb_count;
static int stat_cb_count;
static int rename_cb_count;
static int fsync_cb_count;
static int fdatasync_cb_count;
static int ftruncate_cb_count;
static int sendfile_cb_count;
static int fstat_cb_count;
static int access_cb_count;
static int chmod_cb_count;
static int fchmod_cb_count;
static int chown_cb_count;
static int fchown_cb_count;
static int link_cb_count;
static int symlink_cb_count;
static int readlink_cb_count;
static int utime_cb_count;
static int futime_cb_count;

static uv_loop_t* loop;

static uv_fs_t open_req1;
static uv_fs_t open_req2;
static uv_fs_t read_req;
static uv_fs_t write_req;
static uv_fs_t unlink_req;
static uv_fs_t close_req;
static uv_fs_t mkdir_req;
static uv_fs_t mkdtemp_req1;
static uv_fs_t mkdtemp_req2;
static uv_fs_t rmdir_req;
static uv_fs_t scandir_req;
static uv_fs_t stat_req;
static uv_fs_t rename_req;
static uv_fs_t fsync_req;
static uv_fs_t fdatasync_req;
static uv_fs_t ftruncate_req;
static uv_fs_t sendfile_req;
static uv_fs_t utime_req;
static uv_fs_t futime_req;

static char buf[32];
static char buf2[32];
static char test_buf[] = "test-buffer\n";
static char test_buf2[] = "second-buffer\n";
static uv_buf_t iov;

static void check_permission(const char* filename, unsigned int mode) {
  int r;
  uv_fs_t req;
  uv_stat_t* s;

  r = uv_fs_stat(uv_default_loop(), &req, filename, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);

  s = &req.statbuf;
  ASSERT((s->st_mode & 0777) == mode);

  uv_fs_req_cleanup(&req);
}


static void dummy_cb(uv_fs_t* req) {
  (void) req;
  dummy_cb_count++;
}


static void link_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_LINK);
  ASSERT(req->result == 0);
  link_cb_count++;
  uv_fs_req_cleanup(req);
}


static void symlink_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_SYMLINK);
  ASSERT(req->result == 0);
  symlink_cb_count++;
  uv_fs_req_cleanup(req);
}

static void readlink_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_READLINK);
  ASSERT(req->result == 0);
  ASSERT(strcmp((const char*)req->ptr, "test_file_symlink2") == 0);
  readlink_cb_count++;
  uv_fs_req_cleanup(req);
}


static void access_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_ACCESS);
  access_cb_count++;
  uv_fs_req_cleanup(req);
}


static void fchmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_FCHMOD);
  ASSERT(req->result == 0);
  fchmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", *(int*)req->data);
}


static void chmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHMOD);
  ASSERT(req->result == 0);
  chmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", *(int*)req->data);
}


static void fchown_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_FCHOWN);
  ASSERT(req->result == 0);
  fchown_cb_count++;
  uv_fs_req_cleanup(req);
}


static void chown_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
  ASSERT(req->result == 0);
  chown_cb_count++;
  uv_fs_req_cleanup(req);
}

static void chown_root_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
#ifdef _WIN32
  /* On windows, chown is a no-op and always succeeds. */
  ASSERT(req->result == 0);
#else
  /* On unix, chown'ing the root directory is not allowed -
   * unless you're root, of course.
   */
  if (geteuid() == 0)
    ASSERT(req->result == 0);
  else
    ASSERT(req->result == UV_EPERM);
#endif
  chown_cb_count++;
  uv_fs_req_cleanup(req);
}

static void unlink_cb(uv_fs_t* req) {
  ASSERT(req == &unlink_req);
  ASSERT(req->fs_type == UV_FS_UNLINK);
  ASSERT(req->result == 0);
  unlink_cb_count++;
  uv_fs_req_cleanup(req);
}

static void fstat_cb(uv_fs_t* req) {
  uv_stat_t* s = (uv_stat_t*)req->ptr;
  ASSERT(req->fs_type == UV_FS_FSTAT);
  ASSERT(req->result == 0);
  ASSERT(s->st_size == sizeof(test_buf));
  uv_fs_req_cleanup(req);
  fstat_cb_count++;
}


static void close_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &close_req);
  ASSERT(req->fs_type == UV_FS_CLOSE);
  ASSERT(req->result == 0);
  close_cb_count++;
  uv_fs_req_cleanup(req);
  if (close_cb_count == 3) {
    r = uv_fs_unlink(loop, &unlink_req, "test_file2", unlink_cb);
    ASSERT(r == 0);
  }
}


static void ftruncate_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &ftruncate_req);
  ASSERT(req->fs_type == UV_FS_FTRUNCATE);
  ASSERT(req->result == 0);
  ftruncate_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  ASSERT(r == 0);
}


static void read_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &read_req);
  ASSERT(req->fs_type == UV_FS_READ);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  read_cb_count++;
  uv_fs_req_cleanup(req);
  if (read_cb_count == 1) {
    ASSERT(strcmp(buf, test_buf) == 0);
    r = uv_fs_ftruncate(loop, &ftruncate_req, open_req1.result, 7,
                        ftruncate_cb);
    ASSERT(r == 0);
  } else {
    ASSERT(strcmp(buf, "test-bu") == 0);
    r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  }
  ASSERT(r == 0);
}


static void open_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  if (req->result < 0) {
    fprintf(stderr, "async open error: %d\n", (int) req->result);
    ASSERT(0);
  }
  open_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_file2\0", 11) == 0);
  uv_fs_req_cleanup(req);
  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      read_cb);
  ASSERT(r == 0);
}


static void open_cb_simple(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  if (req->result < 0) {
    fprintf(stderr, "async open error: %d\n", (int) req->result);
    ASSERT(0);
  }
  open_cb_count++;
  ASSERT(req->path);
  uv_fs_req_cleanup(req);
}


static void fsync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fsync_req);
  ASSERT(req->fs_type == UV_FS_FSYNC);
  ASSERT(req->result == 0);
  fsync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  ASSERT(r == 0);
}


static void fdatasync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fdatasync_req);
  ASSERT(req->fs_type == UV_FS_FDATASYNC);
  ASSERT(req->result == 0);
  fdatasync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fsync(loop, &fsync_req, open_req1.result, fsync_cb);
  ASSERT(r == 0);
}


static void write_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &write_req);
  ASSERT(req->fs_type == UV_FS_WRITE);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  write_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fdatasync(loop, &fdatasync_req, open_req1.result, fdatasync_cb);
  ASSERT(r == 0);
}


static void create_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result >= 0);
  create_cb_count++;
  uv_fs_req_cleanup(req);
  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, req->result, &iov, 1, -1, write_cb);
  ASSERT(r == 0);
}


static void rename_cb(uv_fs_t* req) {
  ASSERT(req == &rename_req);
  ASSERT(req->fs_type == UV_FS_RENAME);
  ASSERT(req->result == 0);
  rename_cb_count++;
  uv_fs_req_cleanup(req);
}


static void mkdir_cb(uv_fs_t* req) {
  ASSERT(req == &mkdir_req);
  ASSERT(req->fs_type == UV_FS_MKDIR);
  ASSERT(req->result == 0);
  mkdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
}


static void check_mkdtemp_result(uv_fs_t* req) {
  int r;

  ASSERT(req->fs_type == UV_FS_MKDTEMP);
  ASSERT(req->result == 0);
  ASSERT(req->path);
  ASSERT(strlen(req->path) == 15);
  ASSERT(memcmp(req->path, "test_dir_", 9) == 0);
  ASSERT(memcmp(req->path + 9, "XXXXXX", 6) != 0);
  check_permission(req->path, 0700);

  /* Check if req->path is actually a directory */
  r = uv_fs_stat(uv_default_loop(), &stat_req, req->path, NULL);
  ASSERT(r == 0);
  ASSERT(((uv_stat_t*)stat_req.ptr)->st_mode & S_IFDIR);
  uv_fs_req_cleanup(&stat_req);
}


static void mkdtemp_cb(uv_fs_t* req) {
  ASSERT(req == &mkdtemp_req1);
  check_mkdtemp_result(req);
  mkdtemp_cb_count++;
}


static void rmdir_cb(uv_fs_t* req) {
  ASSERT(req == &rmdir_req);
  ASSERT(req->fs_type == UV_FS_RMDIR);
  ASSERT(req->result == 0);
  rmdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
}


/*
static void scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;
  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 2);
  ASSERT(req->ptr);

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
  }
  scandir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
  ASSERT(!req->ptr);
}
*/


/*
static void empty_scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;

  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 0);
  ASSERT(req->ptr == NULL);
  ASSERT(UV_EOF == uv_fs_scandir_next(req, &dent));
  uv_fs_req_cleanup(req);
  scandir_cb_count++;
}
*/


static void file_scandir_cb(uv_fs_t* req) {
  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == UV_ENOTDIR);
  ASSERT(req->ptr == NULL);
  uv_fs_req_cleanup(req);
  scandir_cb_count++;
}


static void stat_cb(uv_fs_t* req) {
  ASSERT(req == &stat_req);
  ASSERT(req->fs_type == UV_FS_STAT || req->fs_type == UV_FS_LSTAT);
  ASSERT(req->result == 0);
  ASSERT(req->ptr);
  stat_cb_count++;
  uv_fs_req_cleanup(req);
  ASSERT(!req->ptr);
}


static void sendfile_cb(uv_fs_t* req) {
  ASSERT(req == &sendfile_req);
  ASSERT(req->fs_type == UV_FS_SENDFILE);
  ASSERT(req->result == 65546);
  sendfile_cb_count++;
  uv_fs_req_cleanup(req);
}


static void open_noent_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENOENT);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}

static void open_nametoolong_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENAMETOOLONG);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}

static void open_loop_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ELOOP);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}


static void check_utime(const char* path, double atime, double mtime) {
  uv_stat_t* s;
  uv_fs_t req;
  int r;

  r = uv_fs_stat(loop, &req, path, NULL);
  ASSERT(r == 0);

  ASSERT(req.result == 0);
  s = &req.statbuf;

  ASSERT(s->st_atim.tv_sec  == atime);
  ASSERT(s->st_mtim.tv_sec  == mtime);

  uv_fs_req_cleanup(&req);
}


static void utime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &utime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_UTIME);

  c = (utime_check_t*)req->data;
  check_utime(c->path, c->atime, c->mtime);

  uv_fs_req_cleanup(req);
  utime_cb_count++;
}


static void futime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &futime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_FUTIME);

  c = (utime_check_t*)req->data;
  check_utime(c->path, c->atime, c->mtime);

  uv_fs_req_cleanup(req);
  futime_cb_count++;
}


TEST_IMPL(fs_file_noent) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "does_not_exist", O_RDONLY, 0, NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "does_not_exist", O_RDONLY, 0, open_noent_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  /* TODO add EACCES test */

  return 0;
}


#define TOO_LONG_NAME_LENGTH 65536

static char name[TOO_LONG_NAME_LENGTH + 1];


TEST_IMPL(fs_file_nametoolong) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  memset(name, 'a', TOO_LONG_NAME_LENGTH);
  name[TOO_LONG_NAME_LENGTH] = 0;

  r = uv_fs_open(loop, &req, name, O_RDONLY, 0, NULL);
  ASSERT(r == UV_ENAMETOOLONG);
  ASSERT(req.result == UV_ENAMETOOLONG);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, name, O_RDONLY, 0, open_nametoolong_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  return 0;
}

/*
TEST_IMPL(fs_file_loop) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  unlink("test_symlink");
  r = uv_fs_symlink(loop, &req, "test_symlink", "test_symlink", 0, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "test_symlink", O_RDONLY, 0, NULL);
  ASSERT(r == UV_ELOOP);
  ASSERT(req.result == UV_ELOOP);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "test_symlink", O_RDONLY, 0, open_loop_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  unlink("test_symlink");

  return 0;
}
*/

TEST_IMPL(fs_file_async) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
                 S_IRUSR | S_IWUSR, create_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(fsync_cb_count == 1);
  ASSERT(fdatasync_cb_count == 1);
  ASSERT(close_cb_count == 1);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", rename_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(close_cb_count == 1);
  ASSERT(rename_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDWR, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);
  ASSERT(read_cb_count == 1);
  ASSERT(close_cb_count == 2);
  ASSERT(rename_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  //ASSERT(ftruncate_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 2);
  ASSERT(read_cb_count == 2);
  ASSERT(close_cb_count == 3);
  ASSERT(rename_cb_count == 1);
  ASSERT(unlink_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  //ASSERT(ftruncate_cb_count == 1);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  return 0;
}


TEST_IMPL(fs_file_sync) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_ftruncate(loop, &ftruncate_req, open_req1.result, 7, NULL);
  ASSERT(r == 0);
  ASSERT(ftruncate_req.result == 0);
  uv_fs_req_cleanup(&ftruncate_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, "test-bu") == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_unlink(loop, &unlink_req, "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(unlink_req.result == 0);
  uv_fs_req_cleanup(&unlink_req);

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");

  return 0;
}

TEST_IMPL(fs_file_write_null_buffer) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(write_req.result == 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  unlink("test_file");

  return 0;
}


TEST_IMPL(fs_fstat) {
  int r;
  uv_fs_t req;
  uv_file file;
  uv_stat_t* s;
  struct stat t;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  r = uv_fs_fstat(loop, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  s = (uv_stat_t*)req.ptr;
  ASSERT(s->st_size == sizeof(test_buf));

  r = fstat(file, &t);
  ASSERT(r == 0);

  ASSERT(s->st_dev == (uint64_t) t.st_dev);
  ASSERT(s->st_mode == (uint64_t) t.st_mode);
  ASSERT(s->st_nlink == (uint64_t) t.st_nlink);
  ASSERT(s->st_uid == (uint64_t) t.st_uid);
  ASSERT(s->st_gid == (uint64_t) t.st_gid);
  ASSERT(s->st_rdev == (uint64_t) t.st_rdev);
  ASSERT(s->st_ino == (uint64_t) t.st_ino);
  ASSERT(s->st_size == (uint64_t) t.st_size);
  ASSERT(s->st_blksize == (uint64_t) t.st_blksize);
  ASSERT(s->st_blocks == (uint64_t) t.st_blocks);

  ASSERT(s->st_atim.tv_sec == t.st_atim.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atim.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtim.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtim.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctim.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctim.tv_nsec);

  uv_fs_req_cleanup(&req);

  /* Now do the uv_fs_fstat call asynchronously */
  r = uv_fs_fstat(loop, &req, file, fstat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fstat_cb_count == 1);

  r = uv_fs_close(loop, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");

  return 0;
}


TEST_IMPL(fs_utime) {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_fs_t req;
  int r;

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(loop, &req, path, O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */
  r = uv_fs_utime(loop, &req, path, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(loop, &req, path, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  check_utime(path, atime, mtime);
  uv_fs_req_cleanup(&req);


  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 - mees <3 */
  checkme.path = path;
  checkme.atime = atime;
  checkme.mtime = mtime;

  /* async utime */
  utime_req.data = &checkme;
  r = uv_fs_utime(loop, &utime_req, path, atime, mtime, utime_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(utime_cb_count == 1);

  /* Cleanup. */
  unlink(path);

  return 0;
}


TEST_IMPL(fs_futime) {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_file file;
  uv_fs_t req;
  int r;

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(loop, &req, path, O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */

  r = uv_fs_open(loop, &req, path, O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result; /* FIXME probably not how it's supposed to be used */
  uv_fs_req_cleanup(&req);

  r = uv_fs_futime(loop, &req, file, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(loop, &req, path, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  check_utime(path, atime, mtime);
  uv_fs_req_cleanup(&req);

  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 - mees <3 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = path;

  /* async futime */
  futime_req.data = &checkme;
  r = uv_fs_futime(loop, &futime_req, file, atime, mtime, futime_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(futime_cb_count == 1);

  /* Cleanup. */
  unlink(path);

  return 0;
}


TEST_IMPL(fs_stat_missing_path) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  r = uv_fs_stat(loop, &req, "non_existent_file", NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  return 0;
}


TEST_IMPL(fs_open_dir) {
  const char* path;
  uv_fs_t req;
  int r, file;

  path = ".";
  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, path, O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(req.ptr == NULL);
  file = r;
  uv_fs_req_cleanup(&req);

  r = uv_fs_close(loop, &req, file, NULL);
  ASSERT(r == 0);

  r = uv_fs_open(loop, &req, path, O_RDONLY, 0, open_cb_simple);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  return 0;
}


TEST_IMPL(fs_file_open_append) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR | O_APPEND, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDONLY, S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      NULL);
  printf("read = %d\n", r);
  ASSERT(r == 26);
  ASSERT(read_req.result == 26);
  ASSERT(memcmp(buf,
                "test-buffer\n\0test-buffer\n\0",
                sizeof("test-buffer\n\0test-buffer\n\0") - 1) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");

  return 0;
}


TEST_IMPL(fs_read_file_eof) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1,
                 read_req.result, NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");

  return 0;
}