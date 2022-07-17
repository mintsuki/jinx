// Written by czapek1337 (czapek1337@gmail.com)
// Inspired heavily by https://github.com/managarm/cbuildrt/

#define _GNU_SOURCE

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/wait.h>

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

int main(int argc, char *argv[]) {
    int ok = 1;
    const char *err_msg = "";

    char *rootfs = NULL;
    char **mounts = NULL;
    char **envs = NULL;
    char **process_args = NULL;

    int mount_count = 0;
    int mounts_size = 0;

    int env_count = 0;
    int envs_size = 0;

    bool rw_root = false;
    int uid = -1, gid = -1;
    int euid = geteuid();
    int egid = getegid();

    int setgroups_fd = -1;
    int uid_map_fd = -1;
    int gid_map_fd = -1;

    for (int i = 1; i < argc; ) {
        if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--root") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: '%s' requires a value\n", argv[0], argv[i]);
                goto cleanup;
            }

            rootfs = argv[i + 1];
            i += 2;

            if (i < argc - 1 && strcmp(argv[i], "rw") == 0) {
                rw_root = true;
                i++;
            }
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--uid") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: '%s' requires a value\n", argv[0], argv[i]);
                goto cleanup;
            }

            if (sscanf(argv[i + 1], "%d", &uid) != 1) {
                fprintf(stderr, "%s: '%s' is not a valid user ID\n", argv[0], argv[i + 1]);
                goto cleanup;
            }

            i += 2;
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gid") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: '%s' requires a value\n", argv[0], argv[i]);
                goto cleanup;
            }

            if (sscanf(argv[i + 1], "%d", &gid) != 1) {
                fprintf(stderr, "%s: '%s' is not a valid group ID\n", argv[0], argv[i + 1]);
                goto cleanup;
            }

            i += 2;
        } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--mount") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: '%s' requires a value\n", argv[0], argv[i]);
                goto cleanup;
            }

            if (mount_count == mounts_size) {
                mounts_size = mounts_size == 0 ? 16 : mounts_size * 2;
                char **tmp_mounts = realloc(mounts, sizeof(char *) * mounts_size);
                if (tmp_mounts == NULL) {
                    fprintf(stderr, "%s: failed to allocate mounts array\n", argv[0]);
                    goto cleanup;
                }
                mounts = tmp_mounts;
            }

            char *target = argv[i + 1];
            while (*target && *target != ':') {
                target++;
            }

            if (!*target) {
                fprintf(stderr, "%s: mount points need to be provided in the 'source:target' format\n", argv[0]);
                goto cleanup;
            }

            mounts[mount_count++] = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--env") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: '%s' requires a value\n", argv[0], argv[i]);
                goto cleanup;
            }

            if (env_count == envs_size) {
                envs_size = envs_size == 0 ? 16 : envs_size * 2;
                char **tmp_envs = realloc(envs, sizeof(char *) * envs_size);
                if (tmp_envs == NULL) {
                    fprintf(stderr, "%s: failed to allocate environment variables array\n", argv[0]);
                    goto cleanup;
                }
                envs = tmp_envs;
            }

            char *value = argv[i + 1];
            while (*value && *value != '=') {
                value++;
            }

            if (!*value) {
                fprintf(stderr, "%s: environment variables need to be provided in the 'key=value' format\n", argv[0]);
                goto cleanup;
            }

            envs[env_count++] = argv[i + 1];
            i += 2;
        } else if (strcmp(argv[i], "--") == 0) {
            if (i == argc - 1) {
                fprintf(stderr, "%s: at least one trailing argument is required\n", argv[0]);
                goto cleanup;
            }

            process_args = &argv[i + 1];
            break;
        } else {
            fprintf(stderr, "%s: unrecognized option '%s'\n", argv[0], argv[i]);
            goto cleanup;
        }
    }

    if (rootfs == NULL) {
        fprintf(stderr, "%s: root file system path is required\n", argv[0]);
        goto cleanup;
    }

    if (process_args == NULL) {
        fprintf(stderr, "%s: process arguments are requires\n", argv[0]);
        goto cleanup;
    }

    if (uid == -1 || gid == -1) {
        fprintf(stderr, "%s: user and group IDs are both required\n", argv[0]);
        goto cleanup;
    }

    if (unshare(CLONE_NEWUSER | CLONE_NEWPID) < 0) {
        err_msg = "unshare() failure at line " TOSTRING(__LINE__);
        goto errno_error;
    }

    char uid_map[64], gid_map[64];

    int uid_map_len = snprintf(uid_map, 64, "%d %d 1", uid, euid);
    int gid_map_len = snprintf(gid_map, 64, "%d %d 1", gid, egid);

    setgroups_fd = open("/proc/self/setgroups", O_RDWR);
    if (setgroups_fd < 0 || write(setgroups_fd, "deny", 4) < 0) {
        err_msg = "failed to open or write to /proc/self/setgroups at line " TOSTRING(__LINE__);
        goto errno_error;
    }
    close(setgroups_fd);
    setgroups_fd = -1;

    uid_map_fd = open("/proc/self/uid_map", O_RDWR);
    if (uid_map_fd < 0 || write(uid_map_fd, uid_map, uid_map_len) < 0) {
        err_msg = "failed to open or write to /proc/self/uid_map at line " TOSTRING(__LINE__);
        goto errno_error;
    }
    close(uid_map_fd);
    uid_map_fd = -1;

    gid_map_fd = open("/proc/self/gid_map", O_RDWR);
    if (gid_map_fd < 0 || write(gid_map_fd, gid_map, gid_map_len) < 0) {
        err_msg = "failed to open or write to /proc/self/gid_map at line " TOSTRING(__LINE__);
        goto errno_error;
    }
    close(gid_map_fd);
    gid_map_fd = -1;

    if (setuid(uid) < 0 || setgid(gid) < 0) {
        err_msg = "setuid()/setgid() failure at line " TOSTRING(__LINE__);
        goto errno_error;
    }

    int child_pid = fork();
    if (child_pid == 0) {
        if (unshare(CLONE_NEWNS) < 0) {
            err_msg = "unshare() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        if (mount(rootfs, rootfs, NULL, MS_BIND, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        int root_flags = MS_REMOUNT | MS_BIND | MS_NOSUID | MS_NODEV;

        if (!rw_root) {
            root_flags |= MS_RDONLY;
        }

        if (mount(rootfs, rootfs, NULL, root_flags, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        char *dev_overlays[] = { "tty", "null", "zero", "full", "random", "urandom" };
        char target_path[PATH_MAX];

        for (size_t i = 0; i < sizeof(dev_overlays) / sizeof(char *); i++) {
            char source_path[PATH_MAX];

            snprintf(source_path, PATH_MAX, "/dev/%s", dev_overlays[i]);
            snprintf(target_path, PATH_MAX, "%s/dev/%s", rootfs, dev_overlays[i]);

            if (mount(source_path, target_path, NULL, MS_BIND, NULL) < 0) {
                err_msg = "mount() failure at line " TOSTRING(__LINE__);
                goto errno_error;
            }
        }

        snprintf(target_path, PATH_MAX, "%s/etc/resolv.conf", rootfs);
        if (mount("/etc/resolv.conf", target_path, NULL, MS_BIND, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        snprintf(target_path, PATH_MAX, "%s/dev/pts", rootfs);
        if (mount(NULL, target_path, "devpts", 0, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        snprintf(target_path, PATH_MAX, "%s/dev/shm", rootfs);
        if (mount(NULL, target_path, "tmpfs",  0, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        snprintf(target_path, PATH_MAX, "%s/run", rootfs);
        if (mount(NULL, target_path, "tmpfs", 0, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        snprintf(target_path, PATH_MAX, "%s/tmp", rootfs);
        if (mount(NULL, target_path, "tmpfs", 0, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        snprintf(target_path, PATH_MAX, "%s/proc", rootfs);
        if (mount(NULL, target_path, "proc", 0, NULL) < 0) {
            err_msg = "mount() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        for (int i = 0; i < mount_count; i++) {
            char *source = mounts[i];
            char *target = source;

            while (*target && *target != ':') {
                target++;
            }

            *target++ = 0;

            snprintf(target_path, PATH_MAX, "%s/%s", rootfs, target);
            if (mount(source, target_path, NULL, MS_BIND | MS_REC, NULL) < 0) {
                err_msg = "mount() failure at line " TOSTRING(__LINE__);
                goto errno_error;
            }
        }

        if (chroot(rootfs) < 0) {
            err_msg = "chroot() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        if (chdir("/") < 0) {
            err_msg = "chdir() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        int child = fork();
        if (child == 0) {
            setenv("PATH", "/usr/local/sbin:/usr/local/bin:/usr/bin:/usr/bin/site_perl:/usr/bin/vendor_perl:/usr/bin/core_perl", 1);

            for (int i = 0; i < env_count; i++) {
                char *key = envs[i];
                char *value = key;

                while (*value && *value != '=') {
                    value++;
                }

                *value++ = 0;
                setenv(key, value, 1);
            }

            if (execvp(process_args[0], process_args) < 0) {
                err_msg = "execvp() failure at line " TOSTRING(__LINE__);
                goto errno_error;
            }

            __builtin_unreachable();
        } else {
            int exit_code = -1;
            if (waitpid(child, &exit_code, 0) < 0) {
                err_msg = "waitpid() failure at line " TOSTRING(__LINE__);
                goto errno_error;
            }

            ok = WEXITSTATUS(exit_code);
            goto cleanup;
        }

        __builtin_unreachable();
    } else {
        printf("%s: init is %d (outside of namespace)\n", argv[0], child_pid);

        int exit_code = -1;
        if (waitpid(child_pid, &exit_code, 0) < 0) {
            err_msg = "waitpid() failure at line " TOSTRING(__LINE__);
            goto errno_error;
        }

        ok = WEXITSTATUS(exit_code);
        goto cleanup;
    }

errno_error:
    fprintf(stderr, "%s: %s: %s\n", argv[0], err_msg, strerror(errno));

cleanup:
    if (mounts != NULL) {
        free(mounts);
    }
    if (envs != NULL) {
        free(envs);
    }
    if (setgroups_fd >= 0) {
        close(setgroups_fd);
    }
    if (uid_map_fd >= 0) {
        close(uid_map_fd);
    }
    if (gid_map_fd >= 0) {
        close(gid_map_fd);
    }

    return ok;
}
