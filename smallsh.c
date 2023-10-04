#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

void handle_SIGTSTP(int signo);

/* Global Variables. */
int status = 0;
int background = 0;
char pid[32] = {0};
int last_status = 0;
char exit_status[32];
pid_t last_bg_pid = -1;
char *words[MAX_WORDS] = {0};
char * expand(char const *word);
size_t wordsplit(char const *line);

int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "Too many arguments.");
  }

  char *line = NULL;
  size_t n = 0;

  for (;;) {

  prompt:
    /* The prompt: Print a prompt to stderr by expanding the PS1 parameter. */
    if (input == stdin) {
      char *ps1 = getenv("PS1");
      if (ps1 == NULL) ps1 = "";
      fprintf(stderr, "%s", ps1);
      fflush(stderr);
    }

    ssize_t line_len = getline(&line, &n, input);
    if (line_len <= 0) {
      if (line_len == -1) {
        if (feof(input)) {
          // Reached end-of-file
          break;
        } else {
          fprintf(stderr, "smallsh: Error reading input.\n");
          exit(1);
        }
      } else { 
	/* Check for empty input. */
        goto prompt;
      }
    }
  
    size_t nwords = wordsplit(line);


    for (size_t i = 0; i < nwords; ++i) {
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    }
    words[nwords] = NULL;

    /* Execution: Check for built-in commands (exit and cd).
     * cd: takes one argument, else HOME is implied.
     * exit: takes one argument, else “$?” is implied. */
    if (nwords > 0 && strcmp(words[0], "cd") == 0) {
    /* Handle cd command. */
    if (nwords > 2) {
        fprintf(stderr, "smallsh: cd takes at most one argument.\n");
    } else {
        const char *dir;
        if (nwords == 1) {
            dir = getenv("HOME");
            if (dir == NULL) {
                fprintf(stderr, "smallsh: HOME environment variable not set.\n");
                continue; 
            }
        } else {
            dir = words[1];
        }
        if (chdir(dir) != 0) {
            fprintf(stderr, "smallsh: Failed to change directory to %s: %s\n", dir, strerror(errno));
        }
      }
      continue;
    } else if (nwords > 0 && strcmp(words[0], "exit") == 0) {
      /* Handle exit command. */
      if (nwords > 2) {
        fprintf(stderr, "smallsh: exit [status]\n");
      } else {
          int status = last_status;
          if (nwords == 2) {
              char *endptr;
              status = (int)strtol(words[1], &endptr, 10);
              if (*endptr != '\0' || endptr == words[1]) {
                  fprintf(stderr, "smallsh: Invalid status argument.\n");
              }
          }
        fclose(input);
        exit(status);
      }
    }
    else {
      /* Non-Built-in commands. */
      if (nwords > 0 && strcmp(words[nwords - 1], "&") == 0) {
        background = 1; 
        words[nwords - 1] = NULL;
        --nwords;
      }


      pid_t pid = fork();
      if (pid == 0) {
        /* Child process */
        execvp(words[0], words);
        fprintf(stderr, "smallsh: %s: %s\n", words[0], strerror(errno));
        exit(1);
      } else if (pid < 0) {
        /* Fork error */
        fprintf(stderr, "smallsh: %s: %s\n",words[0], strerror(errno));
      } else {
        /* Parent process */
        if (background) {
          last_bg_pid = pid;
          background = 0;
        } else {
          last_bg_pid = -1;
          waitpid(pid, &status, 0);

          if (WIFSTOPPED(status)) {
            kill(pid, SIGCONT);
            fprintf(stderr, "Child process %d stopped. Continuing.\n", pid);
          }
        }
        if (WIFEXITED(status)) {
          last_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
          last_status = 128 + WTERMSIG(status);
        } else {
          last_status = 1;
        }
      }


      /* Free allocated memory. */
      for (size_t i = 0; i < nwords; ++i) {
        free(words[i]);
        words[i] = NULL;
      }
    }
  }
}

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array with
 * pointers to the words, each as an allocated string. */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;
  int background = 0;
  char const *c = line;

  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
   if (*c == '&' && (*(c+1) == '\0' || isspace(*(c+1)))) {
      background = 1;
      break;
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  if (background) {
    // Remove the background operator from the last word
    if (wind > 0) {
      free(words[wind - 1]);
      words[wind - 1] = NULL;
    }
    --wind;
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets start 
 * and end pointers to the start and end of the parameter token. */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a bas string by 
 * appending supplied strings/character ranges to it. */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free. */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
  while (c) {
    if (c == '!') {
      /* “$!” within a word shall be replaced with the process 
       * ID of the most recent background process. */  
      if (last_bg_pid != -1) {
        snprintf(pid, sizeof(pid), "%d", last_bg_pid);
      } else {
        pid[0] = '\0';
      }
      build_str(pid, NULL);
      } else if (c =='$') {
        /* “$$” within a word shall be replaced with the proccess ID
         * of the smallsh process. */
        snprintf(pid, sizeof(pid), "%d", getpid());
        build_str(pid, NULL);
      } else if (c == '?') {
          
          snprintf(exit_status, sizeof(exit_status), "%d", last_status);
          build_str(exit_status, NULL);
      } else if (c == '{') {
        /* "${parameter}" wll be replaced with the value of the 
         * corresponding environment variable named parameter. */
        char named_parameter[MAX_WORDS];
        memset(named_parameter, 0, sizeof(named_parameter));
        strncpy(named_parameter, start + 2, end - start - 3);
        char *results = getenv(named_parameter);
        if (results != NULL) {
          build_str(results, NULL);
        } 
      else build_str("", NULL);
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}

void handle_SIGTSTP(int signo)
  {
    if (last_bg_pid != -1) {
      kill(last_bg_pid, SIGSTOP);
      fprintf(stderr, "Child process %d stopped. Continuing.\n", last_bg_pid);
      last_bg_pid = -1;
    }
  }
