#ifndef __CRASH_DUMP_H__
#define __CRASH_DUMP_H__

#define CRASHDUMP_ALREADY_EXISTS 1
#define CRASHDUMP_NO_ERROR       0
#define CRASHDUMP_CANT_ADD      -1

void crashdump_init(char *);
void crashdump_deinit();
int crashdump_add_file(char *filename, char);
void crashdump_gather_files();

#endif  // __CRASH_DUMP_H__
