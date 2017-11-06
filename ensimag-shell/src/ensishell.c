/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */
struct job {
	pid_t pid;
	char* nom_cmd;
	struct job* next;
};
struct job *JOBS = NULL;

void ajout(pid_t pid, char* nom) {
	struct job * nouv = malloc(sizeof(struct job));
	nouv -> pid = pid;
	printf("%s\n", nom);
	nouv -> nom_cmd = nom;
	nouv -> next = NULL;
	if (JOBS == NULL) {
		JOBS = nouv;
	} else {
		struct job * current = JOBS;
		while (current -> next != NULL) {
			current = current -> next;
		}
		current -> next = nouv;
	}
}

void print_jobs(void) {
	struct job * current = JOBS;
	if (JOBS != NULL) {
		while (current != NULL) {
			printf("PID: %i, CMD: %s \n", current -> pid, current -> nom_cmd);
			current = current -> next;
		}
	} else {
		printf("Aucune tâche en arrière-plan\n");
	}
}

#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);

	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
			terminate(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			// printf("seq[%d]: ", i);
      //                   for (j=0; cmd[j]!=0; j++) {
      //                           printf("'%s' ", cmd[j]);
      //                   }
			// printf("\n");
			if (strcmp(cmd[0],"jobs") == 0) {
				print_jobs();
			} else {
				pid_t pid = fork();
				if (pid == 0) {
					char reponse[60];
					char * chemin = "/bin/";
					char * vide = "";
					if (cmd[0][0] != '/') {
						strcpy(reponse, chemin);
					} else {
						strcpy(reponse, vide);
					}
					strcat(reponse, cmd[0]);
					if (l->bg) {
						setpgid(0,0);
					}
					execvp(reponse, cmd);
				} else {
					if (! l->bg) {
						waitpid(pid, NULL, 0);
					} else {
						ajout(pid, cmd[0]);
					}
				}
			}

		}
	}

}
