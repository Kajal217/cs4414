#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

#define PASS_MSG "TEST:PASS:"
#define FAIL_MSG "TEST:FAIL:"

#define MAX_CHILDREN 16

void setup() {
    mknod("console", 1, 1);
    open("console", O_RDWR);
    dup(0);
    dup(0);
}

void test_simple_crash_no_fork(void (*test_func)(), const char *no_crash_message) {
    test_func();
    printf(1, "%s\n", no_crash_message);
}
      
int test_simple_crash(void (*test_func)(), const char *crash_message, const char *no_crash_message) {
    int fds[2];
    pipe(fds);
    int pid = fork();
    if (pid == 0) {
        /* child process */
        close(1);
        dup(fds[1]);
        test_func();
        write(1, "X", 1);
        exit();
    } else {
        char text[1];
        close(fds[1]);
        int size = read(fds[0], text, 1);
        wait();
        close(fds[0]);
        if (size == 1) {
            printf(1, "%s\n", no_crash_message);
            return 0;
        } else {
            printf(1, "%s\n", crash_message);
            return 1;
        }
    }
}

static unsigned out_of_bounds_offset = 1;
void test_out_of_bounds_internal() {
    volatile char *end_of_heap = sbrk(0);
    (void) end_of_heap[out_of_bounds_offset];
}

int test_out_of_bounds_fork(int offset, const char *crash_message, const char *no_crash_message) {
    out_of_bounds_offset = offset;
    return test_simple_crash(test_out_of_bounds_internal, crash_message, no_crash_message);
}

void test_out_of_bounds_no_fork(int offset, const char *no_crash_message) {
    out_of_bounds_offset = offset;
    test_simple_crash_no_fork(test_out_of_bounds_internal, no_crash_message);
}

int test_allocation_no_fork(int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2, int check_zero) {
    char *old_end_of_heap = sbrk(size);
    char *new_end_of_heap = sbrk(0);
    if (old_end_of_heap == (char*) -1) {
        printf(1, FAIL_MSG "HEAP_ALLOC\n");
        return 0;
    } else if (new_end_of_heap - old_end_of_heap != size) {
        printf(1, FAIL_MSG "HEAP_SIZE\n");
        return 0;
    } else {
        int failed = 0;
        char *place_one = &old_end_of_heap[offset1];
        char *place_two = &old_end_of_heap[offset2];
        int i;
        for (i = 0; i < count1; ++i) {
            if (check_zero && place_one[i] != '\0') {
                failed = 1;
            }
            place_one[i] = 'A';
        }
        for (i = 0; i < count2; ++i) {
            if (check_zero && place_two[i] != '\0') {
                failed = 1;
            }
            place_two[i] = 'B';
        }
        for (i = 0; i < count1; ++i) {
            if (place_one[i] != 'A')
                failed = 1;
        } 
        for (i = 0; i < count2; ++i) {
            if (place_two[i] != 'B')
                failed = 1;
        }
        if (failed) {
            printf(1, FAIL_MSG "HEAP_VALUES\n");
            return 0;
        } else {
            printf(1, PASS_MSG "\n");
            return 1;
        }
    }
}

int test_allocation_fork(int size, const char *describe_size, const char *describe_amount, int offset1, int count1, int offset2, int count2) {
    printf(1, "testing allocating %s and reading/writing to %s segments of it\n", describe_size, describe_amount);
    int fds[2];
    pipe(fds);
    int pid = fork();
    if (pid == 0) {
        /* child process */
        char *old_end_of_heap = sbrk(size);
        char *new_end_of_heap = sbrk(0);
        if (old_end_of_heap == (char*) -1) {
            write(fds[1], "NA", 2);
        } else if (new_end_of_heap - old_end_of_heap != size) {
            write(fds[1], "NS", 2);
        } else {
            int failed = 0;
            char failed_text[2] = "NR";
            char *place_one = &old_end_of_heap[offset1];
            char *place_two = &old_end_of_heap[offset2];
            int i;
            for (i = 0; i < count1; ++i) {
                if (place_one[i] != 0) {
                    failed_text[1] = 'I';
                    failed = 1;
                }
                place_one[i] = 'A';
            }
            for (i = 0; i < count2; ++i) {
                if (place_two[i] != 0) {
                    failed_text[1] = 'I';
                    failed = 1;
                }
                place_two[i] = 'B';
            }
            for (i = 0; i < count1; ++i) {
                if (place_one[i] != 'A')
                    failed = 1;
            } 
            for (i = 0; i < count2; ++i) {
                if (place_two[i] != 'B')
                    failed = 1;
            }
            write(fds[1], failed ? failed_text : "YY", 2);
        }
        exit();
    } else if (pid > 0) {
        char text[10];
        close(fds[1]);
        int size = read(fds[0], text, 10);
        wait();
        close(fds[0]);
        if (size == 2 && text[0] == 'N') {
            if (text[1] == 'A') {
                printf(1, FAIL_MSG "allocating (but not using) %s with sbrk() returned error\n", describe_size);
            } else if (text[1] == 'I') {
                printf(1, FAIL_MSG "allocation initialized to non-zero value\n");
            } else if (text[1] == 'R') {
                printf(1, FAIL_MSG "using %s parts of %s allocation read wrong value\n", describe_amount, describe_size);
            } else if (text[1] == 'S') {
                printf(1, FAIL_MSG "wrong size allocated by %s allocation\n", describe_size);
            } else {
                printf(1, FAIL_MSG "unknown error using %s parts of %s allocation\n", describe_amount, describe_size);
            }
            return 0;
        } else if (size == 0) {
            printf(1, FAIL_MSG "allocating %s and using %s parts of allocation crashed\n", describe_size, describe_amount);
            return 0;
        } else if (size >= 1 && text[0] == 'Y') {
            printf(1, PASS_MSG "allocating %s and using %s parts of allocation passed\n", describe_size, describe_amount );
            return 1;
        } else {
            printf(1, FAIL_MSG "unknown error\n");
            return 0;
        }
    } else {
        printf(1, FAIL_MSG "allocation test: first fork failed\n");
        return 0;
    }
}

void wait_forever() {
  while (1) { sleep(1000); }
}

void test_copy_on_write_main_child(int result_fd, int size, const char *describe_size, int forks) {
  char *old_end_of_heap = sbrk(size);
  char *new_end_of_heap = sbrk(0);
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
      *p = 'A';
  }
  int children[MAX_CHILDREN] = {0};
  if (forks > MAX_CHILDREN) {
    printf(2, "unsupported number of children in test_copy_on_write\n");
  }
  int failed = 0;
  char failed_code = ' ';
  for (int i = 0; i < forks; ++i) {
    int child_fds[2];
    pipe(child_fds);
    children[i] = fork();
    if (children[i] == -1) {
      printf(2, "fork failed\n");
      failed = 1;
      failed_code = 'f';
      break;
    } else if (children[i] == 0) {
      int found_wrong_memory = 0;
      for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
        if (*p != 'A') {
          found_wrong_memory = 1;
        }
      }
      int place_one = size / 2;
      old_end_of_heap[place_one] = 'B' + i;
      int place_two = 4096 * i;
      if (place_two >= size) {
          place_two = size - 1;
      }
      old_end_of_heap[place_two] = 'C';
      int place_three = 4096 * (i - 1);
      if (place_three >= size || place_three < 0) {
          place_three = size - 2;
      }
      int place_four = 4096 * (i + 1);
      if (place_four >= size) {
          place_four = size - 3;
      }
      printf(1, "three: %c; one: %c; four: %c; already_wrong: %d; i: %d\n",
        old_end_of_heap[place_three],
        old_end_of_heap[place_one],
        old_end_of_heap[place_four],
        found_wrong_memory,
        i);
      if (old_end_of_heap[place_three] != 'A' || old_end_of_heap[place_one] != 'B' + i ||
          old_end_of_heap[place_four] != 'A') {
          found_wrong_memory = 1;
      }
      write(child_fds[1], found_wrong_memory ? "-" : "+", 1);
      wait_forever();
    } else {
      char buffer[1] = {'X'};
      read(child_fds[0], buffer, 1);
      if (buffer[0] != '+') {
        failed = 1;
        failed_code = 'c';
      }
      close(child_fds[0]); close(child_fds[1]);
    }
  }
  for (int i = 0; i < forks; ++i) {
    kill(children[i]);
    wait();
  }
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
    if (*p != 'A') {
      failed = 1;
      failed_code = 'p';
    }
  }
  if (failed) {
    char buffer[2] = {'N', ' '};
    buffer[1] = failed_code;
    write(result_fd, buffer, 2);
  } else {
    write(result_fd, "YY", 2);
  }
}

void test_copy_on_write_main_child_alt(int result_fd, int size, const char *describe_size, int forks, int early_term) {
  char *old_end_of_heap = sbrk(size);
  char *new_end_of_heap = sbrk(0);
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
      *p = 'A';
  }
  int children[MAX_CHILDREN] = {0};
  int child_fds[MAX_CHILDREN][2];
  if (forks > MAX_CHILDREN) {
    printf(2, "unsupported number of children in test_copy_on_write\n");
  }
  int failed = 0;
  char failed_code = ' ';
  for (int i = 0; i < forks; ++i) {
    sleep(1);
    pipe(child_fds[i]);
    children[i] = fork();
    if (children[i] == -1) {
      printf(2, "fork failed\n");
      failed = 1;
      failed_code = 'f';
      break;
    } else if (children[i] == 0) {
      int found_wrong_memory = 0;
      for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
        if (*p != 'A') {
          found_wrong_memory = 1;
        }
      }
      int place_one = size / 2;
      old_end_of_heap[place_one] = 'B' + i;
      int place_two = 4096 * i;
      if (place_two >= size) {
          place_two = size - 1;
      }
      old_end_of_heap[place_two] = 'C' + i;
      int place_three = 4096 * (i - 1);
      if (place_three >= size || place_three < 0) {
          place_three = size - 2;
      }
      int place_four = 4096 * (i + 1);
      if (place_four >= size || place_four < 0) {
          place_four = size - 3;
      }
      if (old_end_of_heap[place_three] != 'A' || old_end_of_heap[place_one] != 'B' + i ||
          old_end_of_heap[place_four] != 'A') {
          found_wrong_memory = 1;
      }
      sleep(5);
      if (old_end_of_heap[place_three] != 'A' || 
          old_end_of_heap[place_four] != 'A' ||
          old_end_of_heap[place_two] != 'C' + i || old_end_of_heap[place_one] != 'B' + i) {
          found_wrong_memory = 1;
      }
      write(child_fds[i][1], found_wrong_memory ? "-" : "+", 1);
      if (early_term) {
          exit();
      } else {
          wait_forever();
      }
    }
  }
  for (int i = 0; i < forks; ++i) {
    if (children[i] != -1) {
      char buffer[1] = {'X'};
      read(child_fds[i][0], buffer, 1);
      if (buffer[0] == 'X') {
        failed = 1;
        failed_code = 'P';
      } else if (buffer[0] != '+') {
        failed = 1;
        failed_code = 'c';
      }
      close(child_fds[i][0]); close(child_fds[i][1]);
    }
  }
  for (int i = 0; i < forks; ++i) {
    kill(children[i]);
    wait();
  }
  for (char *p = old_end_of_heap; p < new_end_of_heap; ++p) {
    if (*p != 'A') {
      failed = 1;
      failed_code = 'p';
    }
  }
  if (failed) {
    char buffer[2] = {'N', ' '};
    buffer[1] = failed_code;
    write(result_fd, buffer, 2);
  } else {
    write(result_fd, "YY", 2);
  }
}

int test_copy_on_write_less_forks(int size, const char *describe_size, int forks) {
  int fds[2];
  pipe(fds);
  test_copy_on_write_main_child(fds[1], size, describe_size, forks);
  char text[2] = {'X', 'X'};
  read(fds[0], text, 2);
  close(fds[0]); close(fds[1]);
  if (text[0] == 'X') {
    printf(1, FAIL_MSG "copy on write test failed --- crash?\n");
    return 0;
  } else if (text[0] == 'N') {
    switch (text[1]) {
    case 'f':
      printf(1, FAIL_MSG "copy on write test failed --- fork failed\n");
      break;
    case 'p':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in parent\n");
      break;
    case 'P':
      printf(1, FAIL_MSG "copy on write test failed --- pipe read problem\n");
      break;
    case 'c':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in child\n");
      break;
    default:
      printf(1, FAIL_MSG"copy on write test failed --- unknown reason\n");
      break;
    }
    return 0;
  } else {
    printf(1, PASS_MSG "copy on write test passed --- allocate %s; "
           "fork %d children; read+write small parts in each child\n",
           describe_size, forks);
    return 1;
  }
}

int test_copy_on_write_less_forks_alt(int size, const char *describe_size, int forks, int early_term) {
  int fds[2];
  pipe(fds);
  test_copy_on_write_main_child_alt(fds[1], size, describe_size, forks, early_term);
  char text[2] = {'X', 'X'};
  read(fds[0], text, 2);
  close(fds[0]); close(fds[1]);
  if (text[0] == 'X') {
    printf(1, FAIL_MSG "copy on write test failed --- crash?\n");
    return 0;
  } else if (text[0] == 'N') {
    switch (text[1]) {
    case 'f':
      printf(1, FAIL_MSG "copy on write test failed --- fork failed\n");
      break;
    case 'p':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in parent\n");
      break;
    case 'c':
      printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in child\n");
      break;
    default:
      printf(1, FAIL_MSG"copy on write test failed --- unknown reason\n");
      break;
    }
    return 0;
  } else {
    printf(1, PASS_MSG "copy on write test passed --- allocate %s; "
           "fork %d children; read+write small parts in each child\n",
           describe_size, forks);
    return 1;
  }
}

int _test_copy_on_write(int size,  const char *describe_size, int forks, int use_alt, int early_term, int pre_alloc, const char* describe_prealloc) {
  int fds[2];
  pipe(fds);
  int pid = fork();
  if (pid == 0) {
    if (pre_alloc > 0) {
      sbrk(pre_alloc);
    }
    if (use_alt) {
      test_copy_on_write_main_child_alt(fds[1], size, describe_size, forks, early_term);
    } else {
      test_copy_on_write_main_child(fds[1], size, describe_size, forks);
    }
    exit();
  } else if (pid > 0) {
    printf(1, "running copy on write test: ");
    if (pre_alloc > 0) {
      printf(1, "allocate but do not use %s; ", describe_prealloc);
    }
    printf(1, "allocate and use %s; fork %d children; read+write small parts in each child",
        describe_size, forks);
    if (use_alt) {
      printf(1, " [and try to keep children running in parallel]");
    }
    printf(1, "\n");
    char text[10] = {'X', 'X'};
    close(fds[1]);
    read(fds[0], text, 10);
    wait();
    close(fds[0]);
    if (text[0] == 'X') {
      printf(1, FAIL_MSG "copy on write test failed --- crash?\n");
      return 0;
    } else if (text[0] == 'N') {
      switch (text[1]) {
      case 'f':
        printf(1, FAIL_MSG "copy on write test failed --- fork failed\n");
        break;
      case 'p':
        printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in parent\n");
        break;
      case 'c':
        printf(1, FAIL_MSG "copy on write test failed --- wrong value for memory in child\n");
        break;
      default:
        printf(1, FAIL_MSG "copy on write test failed --- unknown reason\n");
        break;
      }
      return 0;
    } else {
      printf(1, PASS_MSG "copy on write test passed\n");
      return 1;
    }
  } else if (pid == -1) {
     printf(1, FAIL_MSG "copy on write test failed --- first fork failed\n");
  }
  return 0;
}

int test_copy_on_write(int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 0, 0, 0, "");
}

int test_copy_on_write_alloc_unused(int unused_size, const char *describe_unused_size, int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 0, 0, unused_size, describe_unused_size);
}

int test_copy_on_write_alt(int size, const char *describe_size, int forks) {
  return _test_copy_on_write(size, describe_size, forks, 1, 0, 0, "");
}
